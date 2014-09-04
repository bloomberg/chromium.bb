// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_ev_whitelist.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace ct {

namespace internal {

const uint8 kSomeData[] = {0xd5, 0xe2, 0xaf, 0xe5, 0xbb, 0x10, 0x7c, 0xd1};

TEST(BitStreamReaderTest, CanReadSingleByte) {
  BitStreamReader reader(reinterpret_cast<const char*>(kSomeData), 1);
  uint64 v(0);

  EXPECT_EQ(8u, reader.BitsLeft());
  EXPECT_TRUE(reader.ReadBits(8, &v));
  EXPECT_EQ(UINT64_C(0xd5), v);

  EXPECT_FALSE(reader.ReadBits(1, &v));
  EXPECT_EQ(0u, reader.BitsLeft());
}

TEST(BitStreamReaderTest, CanReadSingleBits) {
  const uint64 expected_bits[] = {1, 1, 0, 1, 0, 1, 0, 1,
                                  1, 1, 1, 0, 0, 0, 1, 0};
  BitStreamReader reader(reinterpret_cast<const char*>(kSomeData), 2);
  EXPECT_EQ(16u, reader.BitsLeft());
  uint64 v(0);

  for (int i = 0; i < 16; ++i) {
    EXPECT_TRUE(reader.ReadBits(1, &v));
    EXPECT_EQ(expected_bits[i], v);
  }
  EXPECT_EQ(0u, reader.BitsLeft());
}

TEST(BitStreamReaderTest, CanReadBitGroups) {
  BitStreamReader reader(reinterpret_cast<const char*>(kSomeData), 3);
  EXPECT_EQ(24u, reader.BitsLeft());
  uint64 v(0);
  uint64 res(0);

  EXPECT_TRUE(reader.ReadBits(5, &v));
  res |= v << 19;
  EXPECT_EQ(19u, reader.BitsLeft());
  EXPECT_TRUE(reader.ReadBits(13, &v));
  res |= v << 6;
  EXPECT_EQ(6u, reader.BitsLeft());
  EXPECT_TRUE(reader.ReadBits(6, &v));
  res |= v;
  EXPECT_EQ(UINT64_C(0xd5e2af), res);

  EXPECT_FALSE(reader.ReadBits(1, &v));
}

TEST(BitStreamReaderTest, CanRead64Bit) {
  BitStreamReader reader(reinterpret_cast<const char*>(kSomeData), 8);
  EXPECT_EQ(64u, reader.BitsLeft());
  uint64 v(0);

  EXPECT_TRUE(reader.ReadBits(64, &v));
  EXPECT_EQ(UINT64_C(0xd5e2afe5bb107cd1), v);
}

TEST(BitStreamReaderTest, CanReadUnaryEncodedNumbers) {
  BitStreamReader reader(reinterpret_cast<const char*>(kSomeData), 3);
  const uint64 expected_values[] = {2, 1, 1, 4, 0, 0, 1, 1, 1, 4};
  uint64 v(0);
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(reader.ReadUnaryEncoding(&v));
    EXPECT_EQ(expected_values[i], v) << "Values differ at position " << i;
  }
}

}  // namespace internal

namespace {

const uint8 kFirstHashRaw[] = {0x00, 0x00, 0x03, 0xd7, 0xfc, 0x18, 0x02, 0xcb};
std::string GetFirstHash() {
  return std::string(reinterpret_cast<const char*>(kFirstHashRaw), 8);
}

// Second hash: Diff from first hash is > 2^47
const uint8 kSecondHashRaw[] = {0x00, 0x01, 0x05, 0xd2, 0x58, 0x47, 0xa7, 0xbf};
std::string GetSecondHash() {
  return std::string(reinterpret_cast<const char*>(kSecondHashRaw), 8);
}

// Third hash: Diff from 2nd hash is < 2^47
const uint8 kThirdHashRaw[] = {0x00, 0x01, 0x48, 0x45, 0x8c, 0x53, 0x03, 0x94};
std::string GetThirdHash() {
  return std::string(reinterpret_cast<const char*>(kThirdHashRaw), 8);
}

const uint8 kWhitelistData[] = {
    0x00, 0x00, 0x03, 0xd7, 0xfc, 0x18, 0x02, 0xcb,  // First hash
    0xc0, 0x7e, 0x97, 0x0b, 0xe9, 0x3d, 0x10, 0x9c,
    0xcd, 0x02, 0xd6, 0xf5, 0x40,
};

}  // namespace

TEST(CTEVWhitelistTest, UncompressFailsForTooShortList) {
  // This list does not contain enough bytes even for the first hash.
  std::set<std::string> res;
  EXPECT_FALSE(internal::UncompressEVWhitelist(
      std::string(reinterpret_cast<const char*>(kWhitelistData), 7), &res));
}

TEST(CTEVWhitelistTest, UncompressFailsForTruncatedList) {
  // This list is missing bits for the second part of the diff.
  std::set<std::string> res;
  EXPECT_FALSE(internal::UncompressEVWhitelist(
      std::string(reinterpret_cast<const char*>(kWhitelistData), 14), &res));
}

TEST(CTEVWhitelistTest, UncompressesWhitelistCorrectly) {
  std::set<std::string> res;
  ASSERT_TRUE(internal::UncompressEVWhitelist(
      std::string(reinterpret_cast<const char*>(kWhitelistData),
                  arraysize(kWhitelistData)),
      &res));

  // Ensure first hash is found
  EXPECT_TRUE(res.find(GetFirstHash()) != res.end());
  // Ensure second hash is found
  EXPECT_TRUE(res.find(GetSecondHash()) != res.end());
  // Ensure last hash is found
  EXPECT_TRUE(res.find(GetThirdHash()) != res.end());
  // Ensure that there are exactly 3 hashes.
  EXPECT_EQ(3u, res.size());
}

TEST(CTEVWhitelistTest, CanFindHashInSetList) {
  std::set<std::string> whitelist_data;
  whitelist_data.insert(GetFirstHash());
  internal::SetEVWhitelistData(whitelist_data);

  EXPECT_TRUE(IsCertificateHashInWhitelist(GetFirstHash()));
}

TEST(CTEVWhitelistTest, CannotFindOldHashAfterSetList) {
  std::set<std::string> whitelist_data;
  whitelist_data.insert(GetFirstHash());
  internal::SetEVWhitelistData(whitelist_data);
  EXPECT_TRUE(IsCertificateHashInWhitelist(GetFirstHash()));

  std::set<std::string> new_whitelist_data;
  new_whitelist_data.insert(GetSecondHash());
  internal::SetEVWhitelistData(new_whitelist_data);
  EXPECT_TRUE(IsCertificateHashInWhitelist(GetSecondHash()));
  EXPECT_FALSE(IsCertificateHashInWhitelist(GetFirstHash()));
}

TEST(CTEVWhitelistTest, CorrectlyIdentifiesWhitelistIsInvalid) {
  std::set<std::string> whitelist_data;
  internal::SetEVWhitelistData(whitelist_data);
  EXPECT_FALSE(HasValidEVWhitelist());
}

TEST(CTEVWhitelistTest, CorrectlyIdentifiesWhitelistIsValid) {
  std::set<std::string> whitelist_data;
  whitelist_data.insert(GetFirstHash());
  internal::SetEVWhitelistData(whitelist_data);
  EXPECT_TRUE(HasValidEVWhitelist());
}

}  // namespace ct

}  // namespace net
