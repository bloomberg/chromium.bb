// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_encoding_context.h"

#include <vector>

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Try to process an indexed header with an invalid index. That should
// fail.
TEST(HpackEncodingContextTest, IndexedHeaderInvalid) {
  HpackEncodingContext encoding_context;

  int32 new_index = -1;
  std::vector<uint32> removed_referenced_indices;
  EXPECT_FALSE(
      encoding_context.ProcessIndexedHeader(kuint32max,
                                            &new_index,
                                            &removed_referenced_indices));
}

// Try to process an indexed header with an index for a static
// header. That should succeed and add a mutable copy into the header
// table.
TEST(HpackEncodingContextTest, IndexedHeaderStatic) {
  HpackEncodingContext encoding_context;

  std::string name = encoding_context.GetNameAt(1).as_string();
  std::string value = encoding_context.GetValueAt(1).as_string();
  EXPECT_NE(name, encoding_context.GetNameAt(0));
  EXPECT_NE(value, encoding_context.GetValueAt(0));

  {
    int32 new_index = -1;
    std::vector<uint32> removed_referenced_indices;
    EXPECT_TRUE(
        encoding_context.ProcessIndexedHeader(1,
                                              &new_index,
                                              &removed_referenced_indices));
    EXPECT_EQ(0, new_index);
    EXPECT_TRUE(removed_referenced_indices.empty());
  }
  EXPECT_EQ(name, encoding_context.GetNameAt(0));
  EXPECT_EQ(value, encoding_context.GetValueAt(0));
  EXPECT_TRUE(encoding_context.IsReferencedAt(0));

  {
    int32 new_index = -1;
    std::vector<uint32> removed_referenced_indices;
    EXPECT_TRUE(
        encoding_context.ProcessIndexedHeader(0,
                                              &new_index,
                                              &removed_referenced_indices));
    EXPECT_EQ(0, new_index);
    EXPECT_TRUE(removed_referenced_indices.empty());
  }
  EXPECT_EQ(name, encoding_context.GetNameAt(0));
  EXPECT_EQ(value, encoding_context.GetValueAt(0));
  EXPECT_FALSE(encoding_context.IsReferencedAt(0));
}

// Try to process an indexed header with an index for a static header
// and an encoding context where a copy of that header wouldn't
// fit. That should succeed without making a copy.
TEST(HpackEncodingContextTest, IndexedHeaderStaticCopyDoesNotFit) {
  HpackEncodingContext encoding_context;
  encoding_context.SetMaxSize(0);

  int32 new_index = -1;
  std::vector<uint32> removed_referenced_indices;
  EXPECT_TRUE(
      encoding_context.ProcessIndexedHeader(1,
                                            &new_index,
                                            &removed_referenced_indices));
  EXPECT_EQ(-1, new_index);
  EXPECT_TRUE(removed_referenced_indices.empty());
}

// NOTE: It's too onerous to try to test invalid input to
// ProcessLiteralHeaderWithIncrementalIndexing(); that would require
// making a really large (>4GB of memory) string.

// Try to process a reasonably-sized literal header with incremental
// indexing. It should succeed.
TEST(HpackEncodingContextTest, LiteralHeaderIncrementalIndexing) {
  HpackEncodingContext encoding_context;

  int32 index = -1;
  std::vector<uint32> removed_referenced_indices;
  EXPECT_TRUE(
      encoding_context.ProcessLiteralHeaderWithIncrementalIndexing(
          "name", "value", &index, &removed_referenced_indices));
  EXPECT_EQ(0, index);
  EXPECT_TRUE(removed_referenced_indices.empty());
  EXPECT_EQ("name", encoding_context.GetNameAt(0).as_string());
  EXPECT_EQ("value", encoding_context.GetValueAt(0).as_string());
  EXPECT_TRUE(encoding_context.IsReferencedAt(0));
}

// Try to process a literal header with incremental indexing that is
// too large for the header table. It should succeed without indexing
// into the table.
TEST(HpackEncodingContextTest, LiteralHeaderIncrementalIndexingDoesNotFit) {
  HpackEncodingContext encoding_context;
  encoding_context.SetMaxSize(0);

  int32 index = -1;
  std::vector<uint32> removed_referenced_indices;
  EXPECT_TRUE(
      encoding_context.ProcessLiteralHeaderWithIncrementalIndexing(
          "name", "value", &index, &removed_referenced_indices));
  EXPECT_EQ(-1, index);
  EXPECT_TRUE(removed_referenced_indices.empty());
}

}  // namespace

}  // namespace net
