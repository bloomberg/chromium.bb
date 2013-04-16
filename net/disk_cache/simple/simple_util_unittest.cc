// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/disk_cache/simple/simple_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using disk_cache::simple_util::ConvertEntryHashKeyToHexString;
using disk_cache::simple_util::GetEntryHashKeyAsHexString;
using disk_cache::simple_util::GetEntryHashKeyFromHexString;
using disk_cache::simple_util::GetEntryHashKey;

class SimpleUtilTest : public testing::Test {};

TEST_F(SimpleUtilTest, ConvertEntryHashKeyToHexString) {
  EXPECT_EQ("0000000005f5e0ff",
            ConvertEntryHashKeyToHexString(99999999));
  EXPECT_EQ("7fffffffffffffff",
            ConvertEntryHashKeyToHexString(9223372036854775807UL));
  EXPECT_EQ("8000000000000000",
            ConvertEntryHashKeyToHexString(9223372036854775808UL));
  EXPECT_EQ("ffffffffffffffff",
            ConvertEntryHashKeyToHexString(18446744073709551615UL));
}

TEST_F(SimpleUtilTest, GetEntryHashKey) {
  EXPECT_EQ("7ac408c1dff9c84b",
            GetEntryHashKeyAsHexString("http://www.amazon.com/"));
  EXPECT_EQ(0x7ac408c1dff9c84bUL, GetEntryHashKey("http://www.amazon.com/"));

  EXPECT_EQ("9fe947998c2ccf47",
            GetEntryHashKeyAsHexString("www.amazon.com"));
  EXPECT_EQ(0x9fe947998c2ccf47UL, GetEntryHashKey("www.amazon.com"));

  EXPECT_EQ("0d4b6b5eeea339da", GetEntryHashKeyAsHexString(""));
  EXPECT_EQ(0x0d4b6b5eeea339daUL, GetEntryHashKey(""));

  EXPECT_EQ("a68ac2ecc87dfd04", GetEntryHashKeyAsHexString("http://www.domain.com/uoQ76Kb2QL5hzaVOSAKWeX0W9LfDLqphmRXpsfHN8tgF5lCsfTxlOVWY8vFwzhsRzoNYKhUIOTc5TnUlT0vpdQflPyk2nh7vurXOj60cDnkG3nsrXMhFCsPjhcZAic2jKpF9F9TYRYQwJo81IMi6gY01RK3ZcNl8WGfqcvoZ702UIdetvR7kiaqo1czwSJCMjRFdG6EgMzgXrwE8DYMz4fWqoa1F1c1qwTCBk3yOcmGTbxsPSJK5QRyNea9IFLrBTjfE7ZlN2vZiI7adcDYJef.htm"));

  EXPECT_EQ(0xa68ac2ecc87dfd04UL, GetEntryHashKey("http://www.domain.com/uoQ76Kb2QL5hzaVOSAKWeX0W9LfDLqphmRXpsfHN8tgF5lCsfTxlOVWY8vFwzhsRzoNYKhUIOTc5TnUlT0vpdQflPyk2nh7vurXOj60cDnkG3nsrXMhFCsPjhcZAic2jKpF9F9TYRYQwJo81IMi6gY01RK3ZcNl8WGfqcvoZ702UIdetvR7kiaqo1czwSJCMjRFdG6EgMzgXrwE8DYMz4fWqoa1F1c1qwTCBk3yOcmGTbxsPSJK5QRyNea9IFLrBTjfE7ZlN2vZiI7adcDYJef.htm"));
}

TEST_F(SimpleUtilTest, GetEntryHashKeyFromHexString) {
  uint64 hash_key = 0;
  EXPECT_TRUE(GetEntryHashKeyFromHexString("0000000005f5e0ff", &hash_key));
  EXPECT_EQ(99999999UL, hash_key);

  EXPECT_TRUE(GetEntryHashKeyFromHexString("7ffffffffffffffF", &hash_key));
  EXPECT_EQ(9223372036854775807UL, hash_key);

  EXPECT_TRUE(GetEntryHashKeyFromHexString("8000000000000000", &hash_key));
  EXPECT_EQ(9223372036854775808UL, hash_key);

  EXPECT_TRUE(GetEntryHashKeyFromHexString("FFFFFFFFFFFFFFFF", &hash_key));
  EXPECT_EQ(18446744073709551615UL, hash_key);

  // Wrong hash string size.
  EXPECT_FALSE(GetEntryHashKeyFromHexString("FFFFFFFFFFFFFFF", &hash_key));

  // Wrong hash string size.
  EXPECT_FALSE(GetEntryHashKeyFromHexString("FFFFFFFFFFFFFFFFF", &hash_key));

  EXPECT_FALSE(GetEntryHashKeyFromHexString("iwr8wglhg8*(&1231((", &hash_key));
}
