// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Program to test the SafeBrowsing protocol parsing v2.1.

#include <map>
#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test parsing one add chunk.
TEST(SafeBrowsingProtocolParsingTest, TestAddChunk) {
  std::string add_chunk("a:1:4:35\naaaax1111\0032222333344447777\00288889999");
  add_chunk[13] = '\0';

  // Run the parse.
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      add_chunk.data(),
      static_cast<int>(add_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 1U);
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), 3U);

  EXPECT_EQ(chunks[0].hosts[0].host, 0x61616161);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 0);

  EXPECT_EQ(chunks[0].hosts[1].host, 0x31313131);
  entry = chunks[0].hosts[1].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 3);
  EXPECT_EQ(entry->PrefixAt(0), 0x32323232);
  EXPECT_EQ(entry->PrefixAt(1), 0x33333333);
  EXPECT_EQ(entry->PrefixAt(2), 0x34343434);

  EXPECT_EQ(chunks[0].hosts[2].host, 0x37373737);
  entry = chunks[0].hosts[2].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->PrefixAt(0), 0x38383838);
  EXPECT_EQ(entry->PrefixAt(1), 0x39393939);
}

// Test parsing one add chunk with full hashes.
TEST(SafeBrowsingProtocolParsingTest, TestAddFullChunk) {
  std::string add_chunk("a:1:32:69\naaaa");
  add_chunk.push_back(2);

  SBFullHash full_hash1, full_hash2;
  for (int i = 0; i < 32; ++i) {
    full_hash1.full_hash[i] = i % 2 ? 1 : 2;
    full_hash2.full_hash[i] = i % 2 ? 3 : 4;
  }

  add_chunk.append(full_hash1.full_hash, 32);
  add_chunk.append(full_hash2.full_hash, 32);

  // Run the parse.
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      add_chunk.data(),
      static_cast<int>(add_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 1U);
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), 1U);

  EXPECT_EQ(chunks[0].hosts[0].host, 0x61616161);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_FALSE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_TRUE(entry->FullHashAt(0) == full_hash1);
  EXPECT_TRUE(entry->FullHashAt(1) == full_hash2);
}

// Test parsing multiple add chunks. We'll use the same chunk as above, and add
// one more after it.
TEST(SafeBrowsingProtocolParsingTest, TestAddChunks) {
  std::string add_chunk("a:1:4:35\naaaax1111\0032222333344447777\00288889999"
                        "a:2:4:13\n5555\002ppppgggg");
  add_chunk[13] = '\0';

  // Run the parse.
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      add_chunk.data(),
      static_cast<int>(add_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 2U);
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), 3U);

  EXPECT_EQ(chunks[0].hosts[0].host, 0x61616161);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 0);

  EXPECT_EQ(chunks[0].hosts[1].host, 0x31313131);
  entry = chunks[0].hosts[1].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 3);
  EXPECT_EQ(entry->PrefixAt(0), 0x32323232);
  EXPECT_EQ(entry->PrefixAt(1), 0x33333333);
  EXPECT_EQ(entry->PrefixAt(2), 0x34343434);

  EXPECT_EQ(chunks[0].hosts[2].host, 0x37373737);
  entry = chunks[0].hosts[2].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->PrefixAt(0), 0x38383838);
  EXPECT_EQ(entry->PrefixAt(1), 0x39393939);


  EXPECT_EQ(chunks[1].chunk_number, 2);
  EXPECT_EQ(chunks[1].hosts.size(), 1U);

  EXPECT_EQ(chunks[1].hosts[0].host, 0x35353535);
  entry = chunks[1].hosts[0].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->PrefixAt(0), 0x70707070);
  EXPECT_EQ(entry->PrefixAt(1), 0x67676767);
}

// Test parsing one add chunk where a hostkey spans several entries.
TEST(SafeBrowsingProtocolParsingTest, TestAddBigChunk) {
  std::string add_chunk("a:1:4:1050\naaaaX");
  add_chunk[add_chunk.size() - 1] |= 0xFF;
  for (int i = 0; i < 255; ++i)
    add_chunk.append(base::StringPrintf("%04d", i));

  add_chunk.append("aaaa");
  add_chunk.push_back(5);
  for (int i = 0; i < 5; ++i)
    add_chunk.append(base::StringPrintf("001%d", i));

  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      add_chunk.data(),
      static_cast<int>(add_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 1U);
  EXPECT_EQ(chunks[0].chunk_number, 1);

  EXPECT_EQ(chunks[0].hosts.size(), 2U);

  const SBChunkHost& host0 = chunks[0].hosts[0];
  EXPECT_EQ(host0.host, 0x61616161);
  EXPECT_EQ(host0.entry->prefix_count(), 255);

  const SBChunkHost& host1 = chunks[0].hosts[1];
  EXPECT_EQ(host1.host, 0x61616161);
  EXPECT_EQ(host1.entry->prefix_count(), 5);
}

// Test to make sure we could deal with truncated bin hash chunk.
TEST(SafeBrowsingProtocolParsingTest, TestTruncatedBinHashChunk) {
  // This chunk delares there are 4 prefixes but actually only contains 2.
  const char add_chunk[] = "a:1:4:16\n11112222";
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(safe_browsing_util::kBinHashList,
                                  add_chunk,
                                  static_cast<int>(sizeof(add_chunk)),
                                  &chunks);
  EXPECT_FALSE(result);
  EXPECT_EQ(chunks.size(), 0U);
}

// Test to make sure we could deal with truncated malwarelist chunk.
TEST(SafeBrowsingProtocolParsingTest, TestTruncatedUrlHashChunk) {
  // This chunk delares there are 4 prefixes but actually only contains 2.
  const char add_chunk[] = "a:1:4:21\naaaa\00411112222";
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;

  // For safe_browsing_util::kMalwareList.
  bool result = parser.ParseChunk(safe_browsing_util::kMalwareList,
                                  add_chunk,
                                  static_cast<int>(sizeof(add_chunk)),
                                  &chunks);
  EXPECT_FALSE(result);
  EXPECT_EQ(chunks.size(), 0U);

  // For safe_browsing_util::kPhishingList.
  result = parser.ParseChunk(safe_browsing_util::kPhishingList,
                             add_chunk,
                             static_cast<int>(sizeof(add_chunk)),
                             &chunks);
  EXPECT_FALSE(result);
  EXPECT_EQ(chunks.size(), 0U);

  // For safe_browsing_util::kBinUrlList.
  result = parser.ParseChunk(safe_browsing_util::kBinUrlList,
                             add_chunk,
                             static_cast<int>(sizeof(add_chunk)),
                             &chunks);
  EXPECT_FALSE(result);
  EXPECT_EQ(chunks.size(), 0U);
}

// Test to verify handling of a truncated chunk header.
TEST(SafeBrowsingProtocolParsingTest, TestTruncatedHeader) {
  std::string truncated_chunks("a:1:4:0\na:");

  // Run the parser.
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      truncated_chunks.data(),
      static_cast<int>(truncated_chunks.length()),
      &chunks);
  EXPECT_FALSE(result);
}

// Test parsing one sub chunk.
TEST(SafeBrowsingProtocolParsingTest, TestSubChunk) {
  std::string sub_chunk("s:9:4:59\naaaaxkkkk1111\003"
                        "zzzz2222zzzz3333zzzz4444"
                        "7777\002yyyy8888yyyy9999");
  sub_chunk[13] = '\0';

  // Run the parse.
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      sub_chunk.data(),
      static_cast<int>(sub_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 1U);
  EXPECT_EQ(chunks[0].chunk_number, 9);
  EXPECT_EQ(chunks[0].hosts.size(), 3U);

  EXPECT_EQ(chunks[0].hosts[0].host, 0x61616161);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_TRUE(entry->IsSub());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->chunk_id(), 0x6b6b6b6b);
  EXPECT_EQ(entry->prefix_count(), 0);

  EXPECT_EQ(chunks[0].hosts[1].host, 0x31313131);
  entry = chunks[0].hosts[1].entry;
  EXPECT_TRUE(entry->IsSub());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 3);
  EXPECT_EQ(entry->ChunkIdAtPrefix(0), 0x7a7a7a7a);
  EXPECT_EQ(entry->PrefixAt(0), 0x32323232);
  EXPECT_EQ(entry->ChunkIdAtPrefix(1), 0x7a7a7a7a);
  EXPECT_EQ(entry->PrefixAt(1), 0x33333333);
  EXPECT_EQ(entry->ChunkIdAtPrefix(2), 0x7a7a7a7a);
  EXPECT_EQ(entry->PrefixAt(2), 0x34343434);

  EXPECT_EQ(chunks[0].hosts[2].host, 0x37373737);
  entry = chunks[0].hosts[2].entry;
  EXPECT_TRUE(entry->IsSub());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->ChunkIdAtPrefix(0), 0x79797979);
  EXPECT_EQ(entry->PrefixAt(0), 0x38383838);
  EXPECT_EQ(entry->ChunkIdAtPrefix(1), 0x79797979);
  EXPECT_EQ(entry->PrefixAt(1), 0x39393939);
}

// Test parsing one sub chunk with full hashes.
TEST(SafeBrowsingProtocolParsingTest, TestSubFullChunk) {
  std::string sub_chunk("s:1:32:77\naaaa");
  sub_chunk.push_back(2);

  SBFullHash full_hash1, full_hash2;
  for (int i = 0; i < 32; ++i) {
    full_hash1.full_hash[i] = i % 2 ? 1 : 2;
    full_hash2.full_hash[i] = i % 2 ? 3 : 4;
  }

  sub_chunk.append("yyyy");
  sub_chunk.append(full_hash1.full_hash, 32);
  sub_chunk.append("zzzz");
  sub_chunk.append(full_hash2.full_hash, 32);

  // Run the parse.
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      sub_chunk.data(),
      static_cast<int>(sub_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 1U);
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), 1U);

  EXPECT_EQ(chunks[0].hosts[0].host, 0x61616161);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_TRUE(entry->IsSub());
  EXPECT_FALSE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->ChunkIdAtPrefix(0), 0x79797979);
  EXPECT_TRUE(entry->FullHashAt(0) == full_hash1);
  EXPECT_EQ(entry->ChunkIdAtPrefix(1), 0x7a7a7a7a);
  EXPECT_TRUE(entry->FullHashAt(1) == full_hash2);
}

// Test parsing the SafeBrowsing update response.
TEST(SafeBrowsingProtocolParsingTest, TestChunkDelete) {
  std::string add_del("n:1700\ni:phishy\nad:1-7,43-597,44444,99999\n"
                      "i:malware\nsd:21-27,42,171717\n");

  SafeBrowsingProtocolParser parser;
  int next_query_sec = 0;
  bool reset = false;
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  EXPECT_TRUE(parser.ParseUpdate(add_del.data(),
                                 static_cast<int>(add_del.length()),
                                 &next_query_sec, &reset, &deletes, &urls));

  EXPECT_TRUE(urls.empty());
  EXPECT_FALSE(reset);
  EXPECT_EQ(next_query_sec, 1700);
  EXPECT_EQ(deletes.size(), 2U);

  EXPECT_EQ(deletes[0].chunk_del.size(), 4U);
  EXPECT_TRUE(deletes[0].chunk_del[0] == ChunkRange(1, 7));
  EXPECT_TRUE(deletes[0].chunk_del[1] == ChunkRange(43, 597));
  EXPECT_TRUE(deletes[0].chunk_del[2] == ChunkRange(44444));
  EXPECT_TRUE(deletes[0].chunk_del[3] == ChunkRange(99999));

  EXPECT_EQ(deletes[1].chunk_del.size(), 3U);
  EXPECT_TRUE(deletes[1].chunk_del[0] == ChunkRange(21, 27));
  EXPECT_TRUE(deletes[1].chunk_del[1] == ChunkRange(42));
  EXPECT_TRUE(deletes[1].chunk_del[2] == ChunkRange(171717));

  // An update response with missing list name.

  next_query_sec = 0;
  deletes.clear();
  urls.clear();
  add_del = "n:1700\nad:1-7,43-597,44444,99999\ni:malware\nsd:4,21-27171717\n";
  EXPECT_FALSE(parser.ParseUpdate(add_del.data(),
                                  static_cast<int>(add_del.length()),
                                  &next_query_sec, &reset, &deletes, &urls));
}

// Test parsing the SafeBrowsing update response.
TEST(SafeBrowsingProtocolParsingTest, TestRedirects) {
  std::string redirects("i:goog-malware-shavar\n"
    "u:cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_1\n"
    "u:cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_2\n"
    "u:cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_3\n"
    "u:s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8641-8800:8641-8689,"
    "8691-8731,8733-8786\n");

  SafeBrowsingProtocolParser parser;
  int next_query_sec = 0;
  bool reset = false;
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  EXPECT_TRUE(parser.ParseUpdate(redirects.data(),
                                 static_cast<int>(redirects.length()),
                                 &next_query_sec, &reset, &deletes, &urls));

  EXPECT_FALSE(reset);
  EXPECT_EQ(urls.size(), 4U);
  EXPECT_EQ(urls[0].url,
      "cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_1");
  EXPECT_EQ(urls[1].url,
      "cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_2");
  EXPECT_EQ(urls[2].url,
      "cache.googlevideo.com/safebrowsing/rd/goog-malware-shavar_s_3");
  EXPECT_EQ(urls[3].url,
      "s.ytimg.com/safebrowsing/rd/goog-phish-shavar_a_8641-8800:8641-8689,"
      "8691-8731,8733-8786");
  EXPECT_EQ(next_query_sec, 0);
  EXPECT_TRUE(deletes.empty());
}

// Test parsing various SafeBrowsing protocol headers.
TEST(SafeBrowsingProtocolParsingTest, TestNextQueryTime) {
  std::string headers("n:1800\ni:goog-white-shavar\n");
  SafeBrowsingProtocolParser parser;
  int next_query_sec = 0;
  bool reset = false;
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  EXPECT_TRUE(parser.ParseUpdate(headers.data(),
                                 static_cast<int>(headers.length()),
                                 &next_query_sec, &reset, &deletes, &urls));

  EXPECT_EQ(next_query_sec, 1800);
  EXPECT_FALSE(reset);
  EXPECT_TRUE(deletes.empty());
  EXPECT_TRUE(urls.empty());
}

// Test parsing data from a GetHashRequest
TEST(SafeBrowsingProtocolParsingTest, TestGetHash) {
  std::string get_hash("goog-phish-shavar:19:96\n"
                       "00112233445566778899aabbccddeeff"
                       "00001111222233334444555566667777"
                       "ffffeeeeddddccccbbbbaaaa99998888");
  std::vector<SBFullHashResult> full_hashes;
  SafeBrowsingProtocolParser parser;
  EXPECT_TRUE(parser.ParseGetHash(get_hash.data(),
                                  static_cast<int>(get_hash.length()),
                                  &full_hashes));

  EXPECT_EQ(full_hashes.size(), 3U);
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   "00112233445566778899aabbccddeeff",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[0].list_name, "goog-phish-shavar");
  EXPECT_EQ(memcmp(&full_hashes[1].hash,
                   "00001111222233334444555566667777",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[1].list_name, "goog-phish-shavar");
  EXPECT_EQ(memcmp(&full_hashes[2].hash,
                   "ffffeeeeddddccccbbbbaaaa99998888",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[2].list_name, "goog-phish-shavar");

  // Test multiple lists in the GetHash results.
  std::string get_hash2("goog-phish-shavar:19:32\n"
                        "00112233445566778899aabbccddeeff"
                        "goog-malware-shavar:19:64\n"
                        "cafebeefcafebeefdeaddeaddeaddead"
                        "zzzzyyyyxxxxwwwwvvvvuuuuttttssss");
  EXPECT_TRUE(parser.ParseGetHash(get_hash2.data(),
                                  static_cast<int>(get_hash2.length()),
                                  &full_hashes));

  EXPECT_EQ(full_hashes.size(), 3U);
  EXPECT_EQ(memcmp(&full_hashes[0].hash,
                   "00112233445566778899aabbccddeeff",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[0].list_name, "goog-phish-shavar");
  EXPECT_EQ(memcmp(&full_hashes[1].hash,
                   "cafebeefcafebeefdeaddeaddeaddead",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[1].list_name, "goog-malware-shavar");
  EXPECT_EQ(memcmp(&full_hashes[2].hash,
                   "zzzzyyyyxxxxwwwwvvvvuuuuttttssss",
                   sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[2].list_name, "goog-malware-shavar");
}

TEST(SafeBrowsingProtocolParsingTest, TestGetHashWithUnknownList) {
  std::string hash_response = "goog-phish-shavar:1:32\n"
                              "12345678901234567890123456789012"
                              "googpub-phish-shavar:19:32\n"
                              "09876543210987654321098765432109";
  std::vector<SBFullHashResult> full_hashes;
  SafeBrowsingProtocolParser parser;
  EXPECT_TRUE(parser.ParseGetHash(hash_response.data(),
                                  hash_response.size(),
                                  &full_hashes));

  EXPECT_EQ(full_hashes.size(), 1U);
  EXPECT_EQ(memcmp("12345678901234567890123456789012",
                   &full_hashes[0].hash, sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[0].list_name, "goog-phish-shavar");
  EXPECT_EQ(full_hashes[0].add_chunk_id, 1);

  hash_response += "goog-malware-shavar:7:32\n"
                   "abcdefghijklmnopqrstuvwxyz123457";
  full_hashes.clear();
  EXPECT_TRUE(parser.ParseGetHash(hash_response.data(),
                                  hash_response.size(),
                                  &full_hashes));

  EXPECT_EQ(full_hashes.size(), 2U);
  EXPECT_EQ(memcmp("12345678901234567890123456789012",
                   &full_hashes[0].hash, sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[0].list_name, "goog-phish-shavar");
  EXPECT_EQ(full_hashes[0].add_chunk_id, 1);
  EXPECT_EQ(memcmp("abcdefghijklmnopqrstuvwxyz123457",
                   &full_hashes[1].hash, sizeof(SBFullHash)), 0);
  EXPECT_EQ(full_hashes[1].list_name, "goog-malware-shavar");
  EXPECT_EQ(full_hashes[1].add_chunk_id, 7);
}

TEST(SafeBrowsingProtocolParsingTest, TestFormatHash) {
  SafeBrowsingProtocolParser parser;
  std::vector<SBPrefix> prefixes;
  std::string get_hash;

  prefixes.push_back(0x34333231);
  prefixes.push_back(0x64636261);
  prefixes.push_back(0x73727170);

  parser.FormatGetHash(prefixes, &get_hash);
  EXPECT_EQ(get_hash, "4:12\n1234abcdpqrs");
}

TEST(SafeBrowsingProtocolParsingTest, TestReset) {
  SafeBrowsingProtocolParser parser;
  std::string update("n:1800\ni:phishy\nr:pleasereset\n");

  bool reset = false;
  int next_update = -1;
  std::vector<SBChunkDelete> deletes;
  std::vector<ChunkUrl> urls;
  EXPECT_TRUE(parser.ParseUpdate(update.data(),
                                 static_cast<int>(update.size()),
                                 &next_update, &reset, &deletes, &urls));
  EXPECT_TRUE(reset);
}

// The SafeBrowsing service will occasionally send zero length chunks so that
// client requests will have longer contiguous chunk number ranges, and thus
// reduce the request size.
TEST(SafeBrowsingProtocolParsingTest, TestZeroSizeAddChunk) {
  std::string add_chunk("a:1:4:0\n");
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;

  bool result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      add_chunk.data(),
      static_cast<int>(add_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 1U);
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), 0U);

  // Now test a zero size chunk in between normal chunks.
  chunks.clear();
  std::string add_chunks("a:1:4:18\n1234\001abcd5678\001wxyz"
                         "a:2:4:0\n"
                         "a:3:4:9\ncafe\001beef");
  result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      add_chunks.data(),
      static_cast<int>(add_chunks.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 3U);

  // See that each chunk has the right content.
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), 2U);
  EXPECT_EQ(chunks[0].hosts[0].host, 0x34333231);
  EXPECT_EQ(chunks[0].hosts[0].entry->PrefixAt(0), 0x64636261);
  EXPECT_EQ(chunks[0].hosts[1].host, 0x38373635);
  EXPECT_EQ(chunks[0].hosts[1].entry->PrefixAt(0), 0x7a797877);

  EXPECT_EQ(chunks[1].chunk_number, 2);
  EXPECT_EQ(chunks[1].hosts.size(), 0U);

  EXPECT_EQ(chunks[2].chunk_number, 3);
  EXPECT_EQ(chunks[2].hosts.size(), 1U);
  EXPECT_EQ(chunks[2].hosts[0].host, 0x65666163);
  EXPECT_EQ(chunks[2].hosts[0].entry->PrefixAt(0), 0x66656562);
}

// Test parsing a zero sized sub chunk.
TEST(SafeBrowsingProtocolParsingTest, TestZeroSizeSubChunk) {
  std::string sub_chunk("s:9:4:0\n");
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;

  bool result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      sub_chunk.data(),
      static_cast<int>(sub_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 1U);
  EXPECT_EQ(chunks[0].chunk_number, 9);
  EXPECT_EQ(chunks[0].hosts.size(), 0U);
  chunks.clear();

  // Test parsing a zero sized sub chunk mixed in with content carrying chunks.
  std::string sub_chunks("s:1:4:9\nabcdxwxyz"
                         "s:2:4:0\n"
                         "s:3:4:26\nefgh\0011234pqrscafe\0015678lmno");
  sub_chunks[12] = '\0';

  result = parser.ParseChunk(
      safe_browsing_util::kMalwareList,
      sub_chunks.data(),
      static_cast<int>(sub_chunks.length()),
      &chunks);
  EXPECT_TRUE(result);

  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), 1U);
  EXPECT_EQ(chunks[0].hosts[0].host, 0x64636261);
  EXPECT_EQ(chunks[0].hosts[0].entry->prefix_count(), 0);

  EXPECT_EQ(chunks[1].chunk_number, 2);
  EXPECT_EQ(chunks[1].hosts.size(), 0U);

  EXPECT_EQ(chunks[2].chunk_number, 3);
  EXPECT_EQ(chunks[2].hosts.size(), 2U);
  EXPECT_EQ(chunks[2].hosts[0].host, 0x68676665);
  EXPECT_EQ(chunks[2].hosts[0].entry->prefix_count(), 1);
  EXPECT_EQ(chunks[2].hosts[0].entry->PrefixAt(0), 0x73727170);
  EXPECT_EQ(chunks[2].hosts[0].entry->ChunkIdAtPrefix(0), 0x31323334);
  EXPECT_EQ(chunks[2].hosts[1].host, 0x65666163);
  EXPECT_EQ(chunks[2].hosts[1].entry->prefix_count(), 1);
  EXPECT_EQ(chunks[2].hosts[1].entry->PrefixAt(0), 0x6f6e6d6c);
  EXPECT_EQ(chunks[2].hosts[1].entry->ChunkIdAtPrefix(0), 0x35363738);
}

TEST(SafeBrowsingProtocolParsingTest, TestAddBinHashChunks) {
  std::string add_chunk("a:1:4:16\naaaabbbbccccdddd"
                        "a:2:4:8\n11112222");
  // Run the parse.
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kBinHashList,
      add_chunk.data(),
      static_cast<int>(add_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 2U);
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), 1U);

  EXPECT_EQ(chunks[0].hosts[0].host, 0);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 4);

  EXPECT_EQ(chunks[1].chunk_number, 2);
  EXPECT_EQ(chunks[1].hosts.size(), 1U);

  EXPECT_EQ(chunks[1].hosts[0].host, 0);
  entry = chunks[1].hosts[0].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->PrefixAt(0), 0x31313131);
  EXPECT_EQ(entry->PrefixAt(1), 0x32323232);
}

// Test parsing one add chunk where a hostkey spans several entries.
TEST(SafeBrowsingProtocolParsingTest, TestAddBigBinHashChunk) {
  std::string add_chunk("a:1:4:1028\n");
  for (int i = 0; i < 257; ++i)
    add_chunk.append(base::StringPrintf("%04d", i));

  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kBinHashList,
      add_chunk.data(),
      static_cast<int>(add_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 1U);
  EXPECT_EQ(chunks[0].chunk_number, 1);

  EXPECT_EQ(chunks[0].hosts.size(), 1U);

  const SBChunkHost& host0 = chunks[0].hosts[0];
  EXPECT_EQ(host0.host, 0);
  EXPECT_EQ(host0.entry->prefix_count(), 257);
}

// Test parsing one sub chunk.
TEST(SafeBrowsingProtocolParsingTest, TestSubBinHashChunk) {
  std::string sub_chunk("s:9:4:16\n1111mmmm2222nnnn");

  // Run the parser.
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kBinHashList,
      sub_chunk.data(),
      static_cast<int>(sub_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 1U);
  EXPECT_EQ(chunks[0].chunk_number, 9);
  EXPECT_EQ(chunks[0].hosts.size(), 1U);

  EXPECT_EQ(chunks[0].hosts[0].host, 0);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_TRUE(entry->IsSub());
  EXPECT_TRUE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 2);
  EXPECT_EQ(entry->ChunkIdAtPrefix(0), 0x31313131);
  EXPECT_EQ(entry->PrefixAt(0), 0x6d6d6d6d);
  EXPECT_EQ(entry->ChunkIdAtPrefix(1), 0x32323232);
  EXPECT_EQ(entry->PrefixAt(1), 0x6e6e6e6e);
}

TEST(SafeBrowsingProtocolParsingTest, TestAddDownloadWhitelistChunk) {
  std::string add_chunk("a:1:32:32\nxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                        "a:2:32:64\nyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
                        "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
  // Run the parse.
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kDownloadWhiteList,
      add_chunk.data(),
      static_cast<int>(add_chunk.length()),
      &chunks);
  EXPECT_TRUE(result);
  EXPECT_EQ(chunks.size(), 2U);
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), 1U);
  EXPECT_EQ(chunks[0].hosts[0].host, 0);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_FALSE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 1);
  SBFullHash full;
  memcpy(full.full_hash, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 32);
  EXPECT_TRUE(entry->FullHashAt(0) == full);

  EXPECT_EQ(chunks[1].chunk_number, 2);
  EXPECT_EQ(chunks[1].hosts.size(), 1U);
  EXPECT_EQ(chunks[1].hosts[0].host, 0);
  entry = chunks[1].hosts[0].entry;
  EXPECT_TRUE(entry->IsAdd());
  EXPECT_FALSE(entry->IsPrefix());
  EXPECT_EQ(entry->prefix_count(), 2);
  memcpy(full.full_hash, "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy", 32);
  EXPECT_TRUE(entry->FullHashAt(0) == full);
  memcpy(full.full_hash, "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz", 32);
  EXPECT_TRUE(entry->FullHashAt(1) == full);
}

// Test parsing one sub chunk.
TEST(SafeBrowsingProtocolParsingTest, TestSubDownloadWhitelistChunk) {
  std::string sub_chunk("s:1:32:36\n1111xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

  // Run the parser.
  SafeBrowsingProtocolParser parser;
  SBChunkList chunks;
  bool result = parser.ParseChunk(
      safe_browsing_util::kDownloadWhiteList,
      sub_chunk.data(),
      static_cast<int>(sub_chunk.length()),
      &chunks);
  ASSERT_TRUE(result);
  ASSERT_EQ(chunks.size(), 1U);
  EXPECT_EQ(chunks[0].chunk_number, 1);
  EXPECT_EQ(chunks[0].hosts.size(), 1U);

  EXPECT_EQ(chunks[0].hosts[0].host, 0);
  SBEntry* entry = chunks[0].hosts[0].entry;
  EXPECT_TRUE(entry->IsSub());
  ASSERT_FALSE(entry->IsPrefix());
  ASSERT_EQ(entry->prefix_count(), 1);
  EXPECT_EQ(entry->ChunkIdAtPrefix(0), 0x31313131);
  SBFullHash full;
  memcpy(full.full_hash, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 32);
  EXPECT_TRUE(entry->FullHashAt(0) == full);
}

// There should be one test case for every list added here.  Each list
// chunk data should contain exactly one add chunk and one sub chunk.
TEST(SafeBrowsingProtocolParsingTest, TestAllLists) {
  std::map<std::string, std::string> add_testdata;
  // goog-*-shavar lists have a host-key followed by one or more prefixes.
  add_testdata[safe_browsing_util::kMalwareList] = std::string(
      "a:129057:4:34\n\xd9\xb8\xb2\x14\x00\xe4""B7P\x00\xaf\x91"
      "{\xd7\x01\xad\xa6\xb5""X\xbb\xcf""F\xcf\x00\xf7""A\xbd"
      "p\x00\xab\xd7\x89\xd3\x00", 48);
  add_testdata[safe_browsing_util::kPhishingList] = std::string(
      "a:301377:4:9\n\xdb\xe0\xaa\x8e\x01\x85\xba\xb2\x9e", 22);
  add_testdata[safe_browsing_util::kBinUrlList] = std::string(
      "a:19420:4:18\n_\x92\x9e\xcd\x01\x03""}I\xa2\\3\xe6""h\x01\xee"
      "H\xf6\xe4", 31);
  add_testdata[safe_browsing_util::kSideEffectFreeWhitelist] = std::string(
      "a:1818:4:9\n\x85\xd0\xfe""i\x01""}\x98\xb1\xe5", 20);
  // goog-*-digestvar lists have no host-key data.
  add_testdata[safe_browsing_util::kBinHashList] = std::string(
      "a:5:4:4\nBBBB", 12);
  add_testdata[safe_browsing_util::kExtensionBlacklist] = std::string(
      "a:81:4:8\nhleedfcc", 17);
  // goog-*-sha256 lists have host-keys but they only contains
  // full-length entires.
  add_testdata[safe_browsing_util::kCsdWhiteList] = std::string(
      "a:35:32:37\n\x06\xf9\xb1\xaf\x01\x06\xf9\xb1\xaf""5\xc""9!\x17\x1e"
      "*-\xc9"",*>YSl6\xf9""B\xb8\x96""O\x98""r\xf2\xd5\x8d\xe3""T\x99", 48);
  // goog-*-digest256 has no host-keys and contains only full-length
  // hashes.
  add_testdata[safe_browsing_util::kDownloadWhiteList] = std::string(
      "a:42:32:32\n\xc8\xec\x9f\x9c\x9b\x9a"",\x82""G:F(\xe9\xad\x9c""b$\x8a"
      "\xba""%\x19\xae""c\x03\x87""~\xd1\xd3""bvC\xfd", 43);
  add_testdata[safe_browsing_util::kIPBlacklist] = std::string(
      "a:12:32:32\n8\x99\x17\xda\xec""+i`\x1a\xb3""8pVh\n$\x01\xd1\x12"
      ":\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 43);

  std::map<std::string, std::string> sub_testdata;
  // goog-*-shavar lists have a host-key, add number followed by zero or more
  // prefixes.
  sub_testdata[safe_browsing_util::kMalwareList] = std::string(
      "s:122930:4:9\n\x85""!_\x13\x00\x00\x01\xf1\xa2", 22);
  sub_testdata[safe_browsing_util::kPhishingList] = std::string(
      "s:150181:4:9\nr\x90""zt\x00\x00\x04\x9f""@", 22);
  sub_testdata[safe_browsing_util::kBinUrlList] = std::string(
      "s:3:4:13\nHOST\x01""####BBBB", 22);
  sub_testdata[safe_browsing_util::kSideEffectFreeWhitelist] = std::string(
      "s:4:4:9\nHOST\x00""####", 17);
  // goog-*-digestvar lists have no host-key data.
  sub_testdata[safe_browsing_util::kBinHashList] = std::string(
      "s:5:4:8\n####BBBB", 16);
  sub_testdata[safe_browsing_util::kExtensionBlacklist] = std::string(
      "s:3:4:8\n\x00\x00\x00""%pgkc", 16);
  // goog-*-sha256 lists have host-keys but they only contains
  // full-length entires.
  sub_testdata[safe_browsing_util::kCsdWhiteList] = std::string(
      "s:1:32:41\n\x1a""\"[\n\x01\x00\x00\x00\x15\x1""a\"[\n\xe9\x81""P\x11"
      "LR\xcb""3\x00""B\x90\xb3\x15""K\xf5\xdc\xd0""V\xc2""aI\x1e""-\xc8"
      "\xce"":\t\x01", 51);
  // goog-*-digest256 has no host-keys and contains only full-length
  // hashes.
  sub_testdata[safe_browsing_util::kDownloadWhiteList] = std::string(
      "s:15:32:36\n\x00\x00\x00""-\x12""!\xa2\x8d""z\x80""a\xfb\x14\xff"
      "f\x13\x18\xcc\xdb\xbd\xc0\xb1""~\xd6\x82""[\xf6\xdc\xa1\x81"
      "TI%\xef""C\xab", 47);
  sub_testdata[safe_browsing_util::kIPBlacklist] = std::string(
      "s:13:32:108\n\x00\x00\x00\rL2\x1e\xc6""P3\x94\x1f\xf1""Cv\x18\x9d\xdf"
      "ih4M.c\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\r_"
      "\xfd\xbe""v\x87\xba""'\x1d\x12\r\xc0\xde""B\xc8""{a\xe7\x07""u\x11\x80"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\tz\xfe\xdb"
      "h\t\x08\xc5""dd\x9""ag\xee\xf5\x83\x91""@\xd9\xa4""i*\x80\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00", 120);

  SafeBrowsingProtocolParser parser;
  for (size_t i = 0; i < arraysize(safe_browsing_util::kAllLists); ++i) {
    std::string listname = safe_browsing_util::kAllLists[i];

    {
      ASSERT_EQ(add_testdata.count(listname), 1U)
          << "Missing add chunk test case for Safe Browsing list: " << listname;
      const std::string& chunk_data = add_testdata[listname];
      SBChunkList chunks;
      EXPECT_TRUE(parser.ParseChunk(listname,
                                    chunk_data.data(),
                                    static_cast<int>(chunk_data.length()),
                                    &chunks))
          << "Unable to parse add chunk data for listname: "
          << listname;
      ASSERT_EQ(chunks.size(), 1U);
      EXPECT_TRUE(chunks[0].is_add);
    }
    {
      ASSERT_EQ(sub_testdata.count(listname), 1U)
          << "Missing sub chunk test case for Safe Browsing list: " << listname;
      const std::string& chunk_data = sub_testdata[listname];
      SBChunkList chunks;
      EXPECT_TRUE(parser.ParseChunk(listname,
                                    chunk_data.data(),
                                    static_cast<int>(chunk_data.length()),
                                    &chunks))
          << "Unable to parse sub chunk data for listname: "
          << listname;
      ASSERT_EQ(chunks.size(), 1U);
      EXPECT_FALSE(chunks[0].is_add);
    }
  }
}
