// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/public/txt_record.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {
namespace dnssd {

TEST(TxtRecordTest, TestCaseInsensitivity) {
  DnsSdTxtRecord txt;
  uint8_t data[]{'a', 'b', 'c'};
  EXPECT_TRUE(txt.SetValue("key", data).ok());
  EXPECT_TRUE(txt.GetValue("KEY").is_value());

  EXPECT_TRUE(txt.SetFlag("KEY2", true).ok());
  EXPECT_TRUE(txt.GetFlag("key2").is_value());
  EXPECT_TRUE(txt.GetFlag("key2").value());
}

TEST(TxtRecordTest, TestEmptyValue) {
  DnsSdTxtRecord txt;
  EXPECT_TRUE(txt.SetValue("key", {}).ok());
  EXPECT_TRUE(txt.GetValue("key").is_value());
  EXPECT_EQ(txt.GetValue("key").value().size(), size_t{0});
}

TEST(TxtRecordTest, TestSetAndGetValue) {
  DnsSdTxtRecord txt;
  uint8_t data[]{'a', 'b', 'c'};
  EXPECT_TRUE(txt.SetValue("key", data).ok());
  EXPECT_TRUE(txt.GetValue("key").is_value());
  EXPECT_EQ(txt.GetValue("key").value().size(), size_t{3});
  EXPECT_EQ(txt.GetValue("key").value()[0], 'a');
  EXPECT_EQ(txt.GetValue("key").value()[1], 'b');
  EXPECT_EQ(txt.GetValue("key").value()[2], 'c');

  uint8_t data2[]{'a', 'b'};
  EXPECT_TRUE(txt.SetValue("key", data2).ok());
  EXPECT_TRUE(txt.GetValue("key").is_value());
  EXPECT_EQ(txt.GetValue("key").value().size(), size_t{2});
  EXPECT_EQ(txt.GetValue("key").value()[0], 'a');
  EXPECT_EQ(txt.GetValue("key").value()[1], 'b');
}

TEST(TxtRecordTest, TestClearValue) {
  DnsSdTxtRecord txt;
  uint8_t data[]{'a', 'b', 'c'};
  EXPECT_TRUE(txt.SetValue("key", data).ok());
  txt.ClearValue("key");

  EXPECT_FALSE(txt.GetValue("key").is_value());
}

TEST(TxtRecordTest, TestSetAndGetFlag) {
  DnsSdTxtRecord txt;
  EXPECT_TRUE(txt.SetFlag("key", true).ok());
  EXPECT_TRUE(txt.GetFlag("key").is_value());
  EXPECT_TRUE(txt.GetFlag("key").value());

  EXPECT_TRUE(txt.SetFlag("key", false).ok());
  EXPECT_TRUE(txt.GetFlag("key").is_value());
  EXPECT_FALSE(txt.GetFlag("key").value());
}

TEST(TxtRecordTest, TestClearFlag) {
  DnsSdTxtRecord txt;
  EXPECT_TRUE(txt.SetFlag("key", true).ok());
  txt.ClearFlag("key");

  EXPECT_FALSE(txt.GetFlag("key").value());
}

TEST(TxtRecordTest, TestGettingWrongRecordTypeFails) {
  DnsSdTxtRecord txt;
  uint8_t data[]{'a', 'b', 'c'};
  EXPECT_TRUE(txt.SetValue("key", data).ok());
  EXPECT_TRUE(txt.SetFlag("key2", true).ok());
  EXPECT_FALSE(txt.GetValue("key2").is_value());
}

TEST(TxtRecordTest, TestClearWrongRecordTypeFails) {
  DnsSdTxtRecord txt;
  uint8_t data[]{'a', 'b', 'c'};
  EXPECT_TRUE(txt.SetValue("key", data).ok());
  EXPECT_TRUE(txt.SetFlag("key2", true).ok());
}

}  // namespace dnssd
}  // namespace discovery
}  // namespace openscreen
