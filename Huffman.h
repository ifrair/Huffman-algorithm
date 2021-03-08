#include "Huffman.h"
#include <iostream>
#include <optional>
#include <queue>
#include <vector>

using namespace Arch;

enum Error {
    OK = 0,
    WRONG_ARGUMENTS,
    UNCORRECT_CODE,
    NO_LETTER
};

static const size_t NUM_BYTE_VALUES = 256;
static const size_t NUM_NODES = 512;

struct Node {
    int num = 0;
    size_t value = 0;
    int child0 = -1;
    int child1 = -1;
    unsigned char letter = 0;
    bool operator<(const Node& right) const {
        if (value == right.value) {
            return num > right.num;
        }
        return value > right.value;
    }
};

void find_value(Node& cur_node, std::vector<std::vector<bool>>& codes, std::vector<bool> cur_codes, std::vector<Node>& nodes) {
    if (cur_node.child0 == -1 && cur_node.child1 == -1) {
        codes[cur_node.letter] = cur_codes;
        return;
    }
    if (cur_node.child0 != -1) {
        cur_codes.push_back(0);
        find_value(nodes[cur_node.child0], codes, cur_codes, nodes);
        cur_codes.pop_back();
    }
    if (cur_node.child1 != -1) {
        cur_codes.push_back(1);
        find_value(nodes[cur_node.child1], codes, cur_codes, nodes);
        cur_codes.pop_back();
    }
}

std::vector<Node> building_tree(std::vector<size_t>& num_signs) {
    std::priority_queue<Node> q;
    std::vector<Node> nodes(NUM_NODES);
    size_t num_nodes = 0;
    for (size_t letter = 0; letter < NUM_BYTE_VALUES; ++letter) {
        size_t num = num_signs[letter];
        nodes[num_nodes] = (Node{ static_cast<int>(num_nodes), num, -1, -1, static_cast<unsigned char>(letter) });
        ++num_nodes;
        q.push(nodes[num_nodes - 1]);
    }
    while (q.size() > 1) {
        Node x = q.top();
        q.pop();
        Node y = q.top();
        q.pop();
        nodes[num_nodes] = (Node{ static_cast<int>(num_nodes), x.value + y.value, x.num, y.num, 0 });
        ++num_nodes;
        q.push(nodes[num_nodes - 1]);
    }
    return nodes;
}

std::vector<std::vector<bool>> getting_codes(std::vector<size_t>& num_signs) {
    std::vector<Node> nodes = building_tree(num_signs);
    size_t num_nodes = NUM_NODES - 1;
    std::vector<std::vector<bool>> codes(NUM_BYTE_VALUES);
    find_value(nodes[num_nodes - 1], codes, std::vector<bool>(0), nodes);
    return codes;
}

void buffer_add(std::vector<bool>& codes, std::ostream* out, unsigned char& buffer, size_t& buff_size) {
    for (bool bit : codes) {
        buffer *= 2;
        buffer += bit;
        ++buff_size;
        if (buff_size == 8) {
            out->write(reinterpret_cast<char*>(&buffer), sizeof(buffer));
            buffer = 0;
            buff_size = 0;
        }
    }
}

int Arch::compress(std::istream* in, std::ostream* out) {
    if (in == nullptr || out == nullptr) {
        return Error::WRONG_ARGUMENTS;
    }
    in->seekg(0, in->end);
    size_t num_bytes = static_cast<size_t>(in->tellg());
    in->seekg(0, in->beg);
    std::vector<size_t> num_signs(NUM_BYTE_VALUES, 0);
    for (size_t i = 0; i < num_bytes; ++i) {
        in->seekg(i);
        unsigned char sign = static_cast<unsigned char>(in->peek());
        ++num_signs[sign];
    }
    in->seekg(0, in->beg);
    for (size_t i = 0; i < NUM_BYTE_VALUES; ++i) {
        out->write(reinterpret_cast<char*>(&num_signs[i]), sizeof(size_t));
    }
    std::vector<std::vector<bool>> codes = getting_codes(num_signs);
    unsigned char buffer = 0;
    size_t buff_size = 0;
    for (size_t i = 0; i < num_bytes; ++i) {
        unsigned char sign;
        in->read(reinterpret_cast<char*>(&sign), sizeof(char));
        buffer_add(codes[sign], out, buffer, buff_size);
    }
    if (buff_size != 0) {
        buffer <<= (8 - buff_size);
        out->write(reinterpret_cast<char*>(&buffer), sizeof(buffer));
    }
    return Error::OK;
}

std::optional<unsigned char> move_with(Node& cur_node, bool bit, std::vector<Node>& nodes) {
    if (!bit && cur_node.child0 != -1) {
        cur_node = nodes[cur_node.child0];
    }
    else if (bit && cur_node.child1 != -1) {
        cur_node = nodes[cur_node.child1];
    }
    if (cur_node.child0 == -1 && cur_node.child1 == -1) {
        return cur_node.letter;
    }
    return std::nullopt;
}

int Arch::decompress(std::istream* in, std::ostream* out) {
    if (in == nullptr || out == nullptr) {
        return Error::WRONG_ARGUMENTS;
    }
    in->seekg(0, in->end);
    size_t num_bytes = static_cast<size_t>(in->tellg());
    in->seekg(0, in->beg);
    std::vector<size_t> num_signs(NUM_BYTE_VALUES);
    size_t signsSum = 0;
    for (size_t i = 0; i < NUM_BYTE_VALUES; ++i) {
        in->read(reinterpret_cast<char*>(&num_signs[i]), sizeof(size_t));
        signsSum += num_signs[i];
    }
    std::vector<Node> nodes = building_tree(num_signs);
    size_t num_nodes = NUM_NODES - 1;
    Node start = nodes[num_nodes - 1];
    Node cur_node = start;
    for (size_t i = NUM_BYTE_VALUES * sizeof(size_t); i < num_bytes; ++i) {
        unsigned char byte;
        in->read(reinterpret_cast<char*>(&byte), sizeof(char));
        for (int j = 7; j >= 0; --j) {
            bool bit = (1 & (byte >> j));
            std::optional<unsigned char> letter = move_with(cur_node, bit, nodes);
            if (letter == std::nullopt) {
                continue;
            }
            out->write(reinterpret_cast<char*>(&letter), sizeof(char));
            cur_node = start;
            --signsSum;
            if (signsSum == 0) {
                if (num_bytes - i > 1) {
                    return Error::UNCORRECT_CODE;
                }
                return Error::OK;
            }
        }
    }
    if (signsSum != 0) {
        return Error::UNCORRECT_CODE;
    }
    return Error::OK;
}
