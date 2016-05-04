// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_header_block.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "net/log/net_log.h"
#include "net/spdy/spdy_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::make_pair;
using std::string;
using ::testing::ElementsAre;

namespace net {

std::pair<base::StringPiece, base::StringPiece> Pair(base::StringPiece k,
                                                     base::StringPiece v) {
  return make_pair(k, v);
}

// This test verifies that SpdyHeaderBlock behaves correctly when empty.
TEST(SpdyHeaderBlockTest, EmptyBlock) {
  SpdyHeaderBlock block;
  EXPECT_TRUE(block.empty());
  EXPECT_EQ(0u, block.size());
  EXPECT_EQ(block.end(), block.find("foo"));
  EXPECT_EQ("", block.GetHeader("foo"));
  EXPECT_TRUE(block.end() == block.begin());

  // Should have no effect.
  block.erase("bar");
}

// This test verifies that headers can be set in a variety of ways.
TEST(SpdyHeaderBlockTest, AddHeaders) {
  SpdyHeaderBlock block1;
  block1["foo"] = string(300, 'x');
  block1["bar"] = "baz";
  block1.ReplaceOrAppendHeader("qux", "qux1");
  block1["qux"] = "qux2";
  block1.insert(make_pair("key", "value"));

  EXPECT_EQ(Pair("foo", string(300, 'x')), *block1.find("foo"));
  EXPECT_EQ("baz", block1["bar"]);
  EXPECT_EQ("baz", block1.GetHeader("bar"));
  string qux("qux");
  EXPECT_EQ("qux2", block1[qux]);
  EXPECT_EQ("qux2", block1.GetHeader(qux));
  EXPECT_EQ(Pair("key", "value"), *block1.find("key"));

  block1.erase("key");
  EXPECT_EQ(block1.end(), block1.find("key"));
  EXPECT_EQ("", block1.GetHeader("key"));
}

// This test verifies that SpdyHeaderBlock can be copied.
TEST(SpdyHeaderBlockTest, CopyBlocks) {
  SpdyHeaderBlock block1;
  block1["foo"] = string(300, 'x');
  block1["bar"] = "baz";
  block1.ReplaceOrAppendHeader("qux", "qux1");

  SpdyHeaderBlock block2;
  block2 = block1;

  SpdyHeaderBlock block3(block1);

  EXPECT_EQ(block1, block2);
  EXPECT_EQ(block1, block3);
}

TEST(SpdyHeaderBlockTest, ToNetLogParamAndBackAgain) {
  SpdyHeaderBlock headers;
  headers["A"] = "a";
  headers["B"] = "b";

  std::unique_ptr<base::Value> event_param(SpdyHeaderBlockNetLogCallback(
      &headers, NetLogCaptureMode::IncludeCookiesAndCredentials()));

  SpdyHeaderBlock headers2;
  ASSERT_TRUE(SpdyHeaderBlockFromNetLogParam(event_param.get(), &headers2));
  EXPECT_EQ(headers, headers2);
}

TEST(SpdyHeaderBlockTest, Equality) {
  // Test equality and inequality operators.
  SpdyHeaderBlock block1;
  block1["foo"] = "bar";

  SpdyHeaderBlock block2;
  block2["foo"] = "bar";

  SpdyHeaderBlock block3;
  block3["baz"] = "qux";

  EXPECT_EQ(block1, block2);
  EXPECT_NE(block1, block3);
}

}  // namespace net
