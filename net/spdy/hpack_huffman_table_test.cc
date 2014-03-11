// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_huffman_table.h"

#include <bitset>
#include <string>

#include "base/logging.h"
#include "net/spdy/hpack_constants.h"
#include "net/spdy/hpack_input_stream.h"
#include "net/spdy/hpack_output_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::string;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Pointwise;

namespace net {

namespace test {

typedef HpackHuffmanTable::DecodeEntry DecodeEntry;
typedef HpackHuffmanTable::DecodeTable DecodeTable;

class HpackHuffmanTablePeer {
 public:
  explicit HpackHuffmanTablePeer(const HpackHuffmanTable& table)
      : table_(table) { }

  const std::vector<uint32>& code_by_id() const {
    return table_.code_by_id_;
  }
  const std::vector<uint8>& length_by_id() const {
    return table_.length_by_id_;
  }
  const std::vector<DecodeTable>& decode_tables() const {
    return table_.decode_tables_;
  }
  char pad_bits() const {
    // Cast to match signed-ness of bits8().
    return static_cast<char>(table_.pad_bits_);
  }
  uint16 failed_symbol_id() const {
    return table_.failed_symbol_id_;
  }
  std::vector<DecodeEntry> decode_entries(const DecodeTable& decode_table) {
    std::vector<DecodeEntry>::const_iterator begin =
        table_.decode_entries_.begin() + decode_table.entries_offset;
    return std::vector<DecodeEntry>(begin, begin + decode_table.size());
  }
  void DumpDecodeTable(const DecodeTable& table) {
    std::vector<DecodeEntry> entries = decode_entries(table);
    LOG(INFO) << "Table size " << (1 << table.indexed_length)
              << " prefix " << unsigned(table.prefix_length)
              << " indexed " << unsigned(table.indexed_length);
    size_t i = 0;
    while (i != table.size()) {
      const DecodeEntry& entry = entries[i];
      LOG(INFO) << i << ":"
                << " next_table " << unsigned(entry.next_table_index)
                << " length " << unsigned(entry.length)
                << " symbol " << unsigned(entry.symbol_id);
      size_t j = 1;
      for (; (i + j) != table.size(); j++) {
        const DecodeEntry& next = entries[i + j];
        if (next.next_table_index != entry.next_table_index ||
            next.length != entry.length ||
            next.symbol_id != entry.symbol_id)
          break;
      }
      if (j > 1) {
        LOG(INFO) << "  (repeats " << j << " times)";
      }
      i += j;
    }
  }

 private:
  const HpackHuffmanTable& table_;
};

namespace {

MATCHER(DecodeEntryEq, "") {
  const DecodeEntry& lhs = std::tr1::get<0>(arg);
  const DecodeEntry& rhs = std::tr1::get<1>(arg);
  return lhs.next_table_index == rhs.next_table_index &&
      lhs.length == rhs.length &&
      lhs.symbol_id == rhs.symbol_id;
}

uint32 bits32(const string& bitstring) {
  return std::bitset<32>(bitstring).to_ulong();
}
char bits8(const string& bitstring) {
  return static_cast<char>(std::bitset<8>(bitstring).to_ulong());
}

TEST(HpackHuffmanTableTest, InitializeHpackCode) {
  HpackHuffmanTable table;
  std::vector<HpackHuffmanSymbol> code = HpackHuffmanCode();
  EXPECT_TRUE(table.Initialize(&code[0], code.size()));
  EXPECT_TRUE(table.IsInitialized());
  EXPECT_EQ(HpackHuffmanTablePeer(table).pad_bits(),
            bits8("11111111"));  // First 8 bits of EOS.
}

TEST(HpackHuffmanTableTest, InitializeEdgeCases) {
  {
    // Verify eight symbols can be encoded with 3 bits per symbol.
    HpackHuffmanSymbol code[] = {
      {bits32("00000000000000000000000000000000"), 3, 0},
      {bits32("00100000000000000000000000000000"), 3, 1},
      {bits32("01000000000000000000000000000000"), 3, 2},
      {bits32("01100000000000000000000000000000"), 3, 3},
      {bits32("10000000000000000000000000000000"), 3, 4},
      {bits32("10100000000000000000000000000000"), 3, 5},
      {bits32("11000000000000000000000000000000"), 3, 6},
      {bits32("11100000000000000000000000000000"), 8, 7}};
    HpackHuffmanTable table;
    EXPECT_TRUE(table.Initialize(code, arraysize(code)));
  }
  {
    // But using 2 bits with one symbol overflows the code.
    HpackHuffmanSymbol code[] = {
      {bits32("01000000000000000000000000000000"), 3, 0},
      {bits32("01100000000000000000000000000000"), 3, 1},
      {bits32("00000000000000000000000000000000"), 2, 2},
      {bits32("10000000000000000000000000000000"), 3, 3},
      {bits32("10100000000000000000000000000000"), 3, 4},
      {bits32("11000000000000000000000000000000"), 3, 5},
      {bits32("11100000000000000000000000000000"), 3, 6},
      {bits32("00000000000000000000000000000000"), 8, 7}};  // Overflow.
    HpackHuffmanTable table;
    EXPECT_FALSE(table.Initialize(code, arraysize(code)));
    EXPECT_EQ(7, HpackHuffmanTablePeer(table).failed_symbol_id());
  }
  {
    // Verify four symbols can be encoded with incremental bits per symbol.
    HpackHuffmanSymbol code[] = {
      {bits32("00000000000000000000000000000000"), 1, 0},
      {bits32("10000000000000000000000000000000"), 2, 1},
      {bits32("11000000000000000000000000000000"), 3, 2},
      {bits32("11100000000000000000000000000000"), 8, 3}};
    HpackHuffmanTable table;
    EXPECT_TRUE(table.Initialize(code, arraysize(code)));
  }
  {
    // But repeating a length overflows the code.
    HpackHuffmanSymbol code[] = {
      {bits32("00000000000000000000000000000000"), 1, 0},
      {bits32("10000000000000000000000000000000"), 2, 1},
      {bits32("11000000000000000000000000000000"), 2, 2},
      {bits32("00000000000000000000000000000000"), 8, 3}};  // Overflow.
    HpackHuffmanTable table;
    EXPECT_FALSE(table.Initialize(code, arraysize(code)));
    EXPECT_EQ(3, HpackHuffmanTablePeer(table).failed_symbol_id());
  }
  {
    // Symbol IDs must be assigned sequentially with no gaps.
    HpackHuffmanSymbol code[] = {
      {bits32("00000000000000000000000000000000"), 1, 0},
      {bits32("10000000000000000000000000000000"), 2, 1},
      {bits32("11000000000000000000000000000000"), 3, 1},  // Repeat.
      {bits32("11100000000000000000000000000000"), 8, 3}};
    HpackHuffmanTable table;
    EXPECT_FALSE(table.Initialize(code, arraysize(code)));
    EXPECT_EQ(2, HpackHuffmanTablePeer(table).failed_symbol_id());
  }
  {
    // Canonical codes must begin with zero.
    HpackHuffmanSymbol code[] = {
      {bits32("10000000000000000000000000000000"), 4, 0},
      {bits32("10010000000000000000000000000000"), 4, 1},
      {bits32("10100000000000000000000000000000"), 4, 2},
      {bits32("10110000000000000000000000000000"), 8, 3}};
    HpackHuffmanTable table;
    EXPECT_FALSE(table.Initialize(code, arraysize(code)));
    EXPECT_EQ(0, HpackHuffmanTablePeer(table).failed_symbol_id());
  }
  {
    // Codes must match the expected canonical sequence.
    HpackHuffmanSymbol code[] = {
      {bits32("00000000000000000000000000000000"), 2, 0},
      {bits32("01000000000000000000000000000000"), 2, 1},
      {bits32("11000000000000000000000000000000"), 2, 2},  // Not canonical.
      {bits32("10000000000000000000000000000000"), 8, 3}};
    HpackHuffmanTable table;
    EXPECT_FALSE(table.Initialize(code, arraysize(code)));
    EXPECT_EQ(2, HpackHuffmanTablePeer(table).failed_symbol_id());
  }
  {
    // At least one code must have a length of 8 bits (to ensure pad-ability).
    HpackHuffmanSymbol code[] = {
      {bits32("00000000000000000000000000000000"), 1, 0},
      {bits32("10000000000000000000000000000000"), 2, 1},
      {bits32("11000000000000000000000000000000"), 3, 2},
      {bits32("11100000000000000000000000000000"), 7, 3}};
    HpackHuffmanTable table;
    EXPECT_FALSE(table.Initialize(code, arraysize(code)));
  }
}

TEST(HpackHuffmanTableTest, ValidateInternalsWithSmallCode) {
  HpackHuffmanSymbol code[] = {
    {bits32("01100000000000000000000000000000"), 4, 0},  // 3rd.
    {bits32("01110000000000000000000000000000"), 4, 1},  // 4th.
    {bits32("00000000000000000000000000000000"), 2, 2},  // 1st assigned code.
    {bits32("01000000000000000000000000000000"), 3, 3},  // 2nd.
    {bits32("10000000000000000000000000000000"), 5, 4},  // 5th.
    {bits32("10001000000000000000000000000000"), 5, 5},  // 6th.
    {bits32("10011000000000000000000000000000"), 8, 6},  // 8th.
    {bits32("10010000000000000000000000000000"), 5, 7}};  // 7th.
  HpackHuffmanTable table;
  EXPECT_TRUE(table.Initialize(code, arraysize(code)));

  HpackHuffmanTablePeer peer(table);
  EXPECT_THAT(peer.code_by_id(), ElementsAre(
      bits32("01100000000000000000000000000000"),
      bits32("01110000000000000000000000000000"),
      bits32("00000000000000000000000000000000"),
      bits32("01000000000000000000000000000000"),
      bits32("10000000000000000000000000000000"),
      bits32("10001000000000000000000000000000"),
      bits32("10011000000000000000000000000000"),
      bits32("10010000000000000000000000000000")));
  EXPECT_THAT(peer.length_by_id(), ElementsAre(
      4, 4, 2, 3, 5, 5, 8, 5));

  EXPECT_EQ(peer.decode_tables().size(), 1u);
  {
    std::vector<DecodeEntry> expected;
    expected.resize(128, DecodeEntry(0, 2, 2));  // Fills 128.
    expected.resize(192, DecodeEntry(0, 3, 3));  // Fills 64.
    expected.resize(224, DecodeEntry(0, 4, 0));  // Fills 32.
    expected.resize(256, DecodeEntry(0, 4, 1));  // Fills 32.
    expected.resize(272, DecodeEntry(0, 5, 4));  // Fills 16.
    expected.resize(288, DecodeEntry(0, 5, 5));  // Fills 16.
    expected.resize(304, DecodeEntry(0, 5, 7));  // Fills 16.
    expected.resize(306, DecodeEntry(0, 8, 6));  // Fills 2.
    expected.resize(512, DecodeEntry());  // Remainder is empty.

    EXPECT_THAT(peer.decode_entries(peer.decode_tables()[0]),
                Pointwise(DecodeEntryEq(), expected));
  }
  EXPECT_EQ(peer.pad_bits(), bits8("10011000"));

  char input_storage[] = {2, 3, 2, 7, 4};
  StringPiece input(input_storage, arraysize(input_storage));
  // By symbol: (2) 00 (3) 010 (2) 00 (7) 10010 (4) 10000 (6 as pad) 1001100.
  char expect_storage[] = {
    bits8("00010001"),
    bits8("00101000"),
    bits8("01001100")};
  StringPiece expect(expect_storage, arraysize(expect_storage));

  string buffer_in, buffer_out;
  HpackOutputStream output_stream(kuint32max);
  table.EncodeString(input, &output_stream);
  output_stream.TakeString(&buffer_in);
  EXPECT_EQ(buffer_in, expect);

  HpackInputStream input_stream(kuint32max, buffer_in);
  EXPECT_TRUE(table.DecodeString(&input_stream, input.size(), &buffer_out));
  EXPECT_EQ(buffer_out, input);
}

TEST(HpackHuffmanTableTest, ValidateMultiLevelDecodeTables) {
  HpackHuffmanSymbol code[] = {
    {bits32("00000000000000000000000000000000"), 6, 0},
    {bits32("00000100000000000000000000000000"), 6, 1},
    {bits32("00001000000000000000000000000000"), 11, 2},
    {bits32("00001000001000000000000000000000"), 11, 3},
    {bits32("00001000010000000000000000000000"), 12, 4},
  };
  HpackHuffmanTable table;
  EXPECT_TRUE(table.Initialize(code, arraysize(code)));

  HpackHuffmanTablePeer peer(table);
  EXPECT_EQ(peer.decode_tables().size(), 2u);
  {
    std::vector<DecodeEntry> expected;
    expected.resize(8, DecodeEntry(0, 6, 0));  // Fills 8.
    expected.resize(16, DecodeEntry(0, 6, 1));  // Fills 8.
    expected.resize(17, DecodeEntry(1, 12, 0));  // Pointer. Fills 1.
    expected.resize(512, DecodeEntry());  // Remainder is empty.

    const DecodeTable& decode_table = peer.decode_tables()[0];
    EXPECT_EQ(decode_table.prefix_length, 0);
    EXPECT_EQ(decode_table.indexed_length, 9);
    EXPECT_THAT(peer.decode_entries(decode_table),
                Pointwise(DecodeEntryEq(), expected));
  }
  {
    std::vector<DecodeEntry> expected;
    expected.resize(2, DecodeEntry(1, 11, 2));  // Fills 2.
    expected.resize(4, DecodeEntry(1, 11, 3));  // Fills 2.
    expected.resize(5, DecodeEntry(1, 12, 4));  // Fills 1.
    expected.resize(8, DecodeEntry());  // Remainder is empty.

    const DecodeTable& decode_table = peer.decode_tables()[1];
    EXPECT_EQ(decode_table.prefix_length, 9);
    EXPECT_EQ(decode_table.indexed_length, 3);
    EXPECT_THAT(peer.decode_entries(decode_table),
                Pointwise(DecodeEntryEq(), expected));
  }
  EXPECT_EQ(peer.pad_bits(), bits8("00001000"));
}

TEST(HpackHuffmanTableTest, DecodeWithBadInput) {
  HpackHuffmanSymbol code[] = {
    {bits32("01100000000000000000000000000000"), 4, 0},
    {bits32("01110000000000000000000000000000"), 4, 1},
    {bits32("00000000000000000000000000000000"), 2, 2},
    {bits32("01000000000000000000000000000000"), 3, 3},
    {bits32("10000000000000000000000000000000"), 5, 4},
    {bits32("10001000000000000000000000000000"), 5, 5},
    {bits32("10011000000000000000000000000000"), 6, 6},
    {bits32("10010000000000000000000000000000"), 5, 7},
    {bits32("10011100000000000000000000000000"), 16, 8}};
  HpackHuffmanTable table;
  EXPECT_TRUE(table.Initialize(code, arraysize(code)));

  string buffer;
  const size_t capacity = 4;
  {
    // This example works: (2) 00 (3) 010 (2) 00 (6) 100110 (pad) 100.
    char input_storage[] = {bits8("00010001"), bits8("00110100")};
    StringPiece input(input_storage, arraysize(input_storage));

    HpackInputStream input_stream(kuint32max, input);
    EXPECT_TRUE(table.DecodeString(&input_stream, capacity, &buffer));
    EXPECT_EQ(buffer, "\x02\x03\x02\x06");
  }
  {
    // Expect to fail on an invalid code prefix.
    // (2) 00 (3) 010 (2) 00 (too-large) 101000 (pad) 100.
    char input_storage[] = {bits8("00010001"), bits8("01000111")};
    StringPiece input(input_storage, arraysize(input_storage));

    HpackInputStream input_stream(kuint32max, input);
    EXPECT_FALSE(table.DecodeString(&input_stream, capacity, &buffer));
    EXPECT_EQ(buffer, "\x02\x03\x02");
  }
  {
    // Repeat the shortest 00 code to overflow |buffer|. Expect to fail.
    std::vector<char> input_storage(1 + capacity / 4, '\0');
    StringPiece input(&input_storage[0], input_storage.size());

    HpackInputStream input_stream(kuint32max, input);
    EXPECT_FALSE(table.DecodeString(&input_stream, capacity, &buffer));

    std::vector<char> expected(capacity, '\x02');
    EXPECT_THAT(buffer, ElementsAreArray(expected));
    EXPECT_EQ(capacity, buffer.size());
  }
  {
    // Expect to fail if more than a byte of unconsumed input remains.
    // (6) 100110 (8 truncated) 1001110000
    char input_storage[] = {bits8("10011010"), bits8("01110000")};
    StringPiece input(input_storage, arraysize(input_storage));

    HpackInputStream input_stream(kuint32max, input);
    EXPECT_FALSE(table.DecodeString(&input_stream, 4, &buffer));
    EXPECT_EQ(buffer, "\x06");
  }
}

TEST(HpackHuffmanTableTest, SpecRequestExamples) {
  const HpackHuffmanTable& table(ObtainHpackHuffmanTable());

  string buffer;
  string test_table[] = {
    "\xdb\x6d\x88\x3e\x68\xd1\xcb\x12\x25\xba\x7f",
    "www.example.com",
    "\x63\x65\x4a\x13\x98\xff",
    "no-cache",
    "\x4e\xb0\x8b\x74\x97\x90\xfa\x7f",
    "custom-key",
    "\x4e\xb0\x8b\x74\x97\x9a\x17\xa8\xff",
    "custom-value",
  };
  // Round-trip each test example.
  for (size_t i = 0; i != arraysize(test_table); i += 2) {
    const string& encoded(test_table[i]);
    const string& decoded(test_table[i+1]);
    HpackInputStream input_stream(kuint32max, encoded);
    HpackOutputStream output_stream(kuint32max);

    buffer.reserve(decoded.size());
    EXPECT_TRUE(table.DecodeString(&input_stream, decoded.size(), &buffer));
    EXPECT_EQ(decoded, buffer);
    table.EncodeString(decoded, &output_stream);
    output_stream.TakeString(&buffer);
    EXPECT_EQ(encoded, buffer);
  }
}

TEST(HpackHuffmanTableTest, SpecResponseExamples) {
  const HpackHuffmanTable& table(ObtainHpackHuffmanTable());

  string buffer;
  string test_table[] = {
    "\x98\xa7",
    "302",
    "\x73\xd5\xcd\x11\x1f",
    "private",
    "\xef\x6b\x3a\x7a\x0e\x6e\x8f\xa2\x63\xd0\x72\x9a\x6e\x83\x97\xd8"
        "\x69\xbd\x87\x37\x47\xbb\xbf\xc7",
    "Mon, 21 Oct 2013 20:13:21 GMT",
    "\xce\x31\x74\x3d\x80\x1b\x6d\xb1\x07\xcd\x1a\x39\x62\x44\xb7\x4f",
    "https://www.example.com",
    "\xc5\xad\xb7\x7f\x87\x6f\xc7\xfb\xf7\xfd\xbf\xbe\xbf\xf3\xf7\xf4"
        "\xfb\x7e\xbb\xbe\x9f\x5f\x87\xe3\x7f\xef\xed\xfa\xee\xfa\x7c\x3f"
        "\x1d\x5d\x1a\x23\xce\x54\x64\x36\xcd\x49\x4b\xd5\xd1\xcc\x5f\x05"
        "\x35\x96\x9b",
    "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1",
  };
  // Round-trip each test example.
  for (size_t i = 0; i != arraysize(test_table); i += 2) {
    const string& encoded(test_table[i]);
    const string& decoded(test_table[i+1]);
    HpackInputStream input_stream(kuint32max, encoded);
    HpackOutputStream output_stream(kuint32max);

    buffer.reserve(decoded.size());
    EXPECT_TRUE(table.DecodeString(&input_stream, decoded.size(), &buffer));
    EXPECT_EQ(decoded, buffer);
    table.EncodeString(decoded, &output_stream);
    output_stream.TakeString(&buffer);
    EXPECT_EQ(encoded, buffer);
  }
}

TEST(HpackHuffmanTableTest, RoundTripIndvidualSymbols) {
  const HpackHuffmanTable& table(ObtainHpackHuffmanTable());

  for (size_t i = 0; i != 256; i++) {
    char c = static_cast<char>(i);
    char storage[3] = {c, c, c};
    StringPiece input(storage, arraysize(storage));

    string buffer_in, buffer_out(input.size(), '\0');
    HpackOutputStream output_stream(kuint32max);
    table.EncodeString(input, &output_stream);
    output_stream.TakeString(&buffer_in);

    HpackInputStream input_stream(kuint32max, buffer_in);
    EXPECT_TRUE(table.DecodeString(&input_stream, input.size(), &buffer_out));
    EXPECT_EQ(input, buffer_out);
  }
}

TEST(HpackHuffmanTableTest, RoundTripSymbolSequence) {
  const HpackHuffmanTable& table(ObtainHpackHuffmanTable());

  char storage[512];
  for (size_t i = 0; i != 256; i++) {
    storage[i] = static_cast<char>(i);
    storage[511 - i] = static_cast<char>(i);
  }
  StringPiece input(storage, arraysize(storage));

  string buffer_in, buffer_out(input.size(), '\0');
  HpackOutputStream output_stream(kuint32max);
  table.EncodeString(input, &output_stream);
  output_stream.TakeString(&buffer_in);

  HpackInputStream input_stream(kuint32max, buffer_in);
  EXPECT_TRUE(table.DecodeString(&input_stream, input.size(), &buffer_out));
  EXPECT_EQ(input, buffer_out);
}

}  // namespace

}  // namespace test

}  // namespace net
