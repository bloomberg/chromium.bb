// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_encoding_context.h"

#include <vector>

#include "base/basictypes.h"
#include "net/spdy/hpack_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace test {

class HpackEncodingContextPeer {
 public:
  explicit HpackEncodingContextPeer(const HpackEncodingContext& context)
      : context_(context) {}

  const HpackHeaderTable& header_table() {
    return context_.header_table_;
  }
  uint32 settings_header_table_size() {
    return context_.settings_header_table_size_;
  }

 private:
  const HpackEncodingContext& context_;
};

}  // namespace test

namespace {

TEST(HpackEncodingContextTest, ApplyHeaderTableSizeSetting) {
  HpackEncodingContext context;
  test::HpackEncodingContextPeer peer(context);

  // Default setting and table size are kDefaultHeaderTableSizeSetting.
  EXPECT_EQ(kDefaultHeaderTableSizeSetting,
            peer.settings_header_table_size());
  EXPECT_EQ(kDefaultHeaderTableSizeSetting,
            peer.header_table().max_size());

  // Applying a larger table size setting doesn't affect the headers table.
  context.ApplyHeaderTableSizeSetting(kDefaultHeaderTableSizeSetting * 2);

  EXPECT_EQ(kDefaultHeaderTableSizeSetting * 2,
            peer.settings_header_table_size());
  EXPECT_EQ(kDefaultHeaderTableSizeSetting,
            peer.header_table().max_size());

  // Applying a smaller size setting does update the headers table.
  context.ApplyHeaderTableSizeSetting(kDefaultHeaderTableSizeSetting / 2);

  EXPECT_EQ(kDefaultHeaderTableSizeSetting / 2,
            peer.settings_header_table_size());
  EXPECT_EQ(kDefaultHeaderTableSizeSetting / 2,
            peer.header_table().max_size());
}

TEST(HpackEncodingContextTest, ProcessContextUpdateNewMaximumSize) {
  HpackEncodingContext context;
  test::HpackEncodingContextPeer peer(context);

  EXPECT_EQ(kDefaultHeaderTableSizeSetting,
            peer.settings_header_table_size());

  // Shrink maximum size by half. Succeeds.
  EXPECT_TRUE(context.ProcessContextUpdateNewMaximumSize(
      kDefaultHeaderTableSizeSetting / 2));
  EXPECT_EQ(kDefaultHeaderTableSizeSetting / 2,
            peer.header_table().max_size());

  // Double maximum size. Succeeds.
  EXPECT_TRUE(context.ProcessContextUpdateNewMaximumSize(
      kDefaultHeaderTableSizeSetting));
  EXPECT_EQ(kDefaultHeaderTableSizeSetting, peer.header_table().max_size());

  // One beyond table size setting. Fails.
  EXPECT_FALSE(context.ProcessContextUpdateNewMaximumSize(
      kDefaultHeaderTableSizeSetting + 1));
  EXPECT_EQ(kDefaultHeaderTableSizeSetting, peer.header_table().max_size());
}

// Try to process an indexed header with an index for a static
// header. That should succeed and add a mutable copy into the header
// table.
TEST(HpackEncodingContextTest, IndexedHeaderStatic) {
  HpackEncodingContext encoding_context;

  std::string name = encoding_context.GetNameAt(2).as_string();
  std::string value = encoding_context.GetValueAt(2).as_string();
  EXPECT_EQ(0u, encoding_context.GetMutableEntryCount());
  EXPECT_NE(name, encoding_context.GetNameAt(1));
  EXPECT_NE(value, encoding_context.GetValueAt(1));

  {
    uint32 new_index = 0;
    std::vector<uint32> removed_referenced_indices;
    EXPECT_TRUE(
        encoding_context.ProcessIndexedHeader(2,
                                              &new_index,
                                              &removed_referenced_indices));
    EXPECT_EQ(1u, new_index);
    EXPECT_TRUE(removed_referenced_indices.empty());
  }
  EXPECT_EQ(1u, encoding_context.GetMutableEntryCount());
  EXPECT_EQ(name, encoding_context.GetNameAt(1));
  EXPECT_EQ(value, encoding_context.GetValueAt(1));
  EXPECT_TRUE(encoding_context.IsReferencedAt(1));

  {
    uint32 new_index = 0;
    std::vector<uint32> removed_referenced_indices;
    EXPECT_TRUE(
        encoding_context.ProcessIndexedHeader(1,
                                              &new_index,
                                              &removed_referenced_indices));
    EXPECT_EQ(1u, new_index);
    EXPECT_TRUE(removed_referenced_indices.empty());
  }
  EXPECT_EQ(1u, encoding_context.GetMutableEntryCount());
  EXPECT_EQ(name, encoding_context.GetNameAt(1));
  EXPECT_EQ(value, encoding_context.GetValueAt(1));
  EXPECT_LE(1u, encoding_context.GetMutableEntryCount());
  EXPECT_FALSE(encoding_context.IsReferencedAt(1));
}

// Try to process an indexed header with an index for a static header
// and an encoding context where a copy of that header wouldn't
// fit. That should succeed without making a copy.
TEST(HpackEncodingContextTest, IndexedHeaderStaticCopyDoesNotFit) {
  HpackEncodingContext encoding_context;
  encoding_context.ProcessContextUpdateNewMaximumSize(0);

  uint32 new_index = 0;
  std::vector<uint32> removed_referenced_indices;
  EXPECT_TRUE(
      encoding_context.ProcessIndexedHeader(1,
                                            &new_index,
                                            &removed_referenced_indices));
  EXPECT_EQ(0u, new_index);
  EXPECT_TRUE(removed_referenced_indices.empty());
  EXPECT_EQ(0u, encoding_context.GetMutableEntryCount());
}

// Add a bunch of new headers and then process a context update to empty the
// reference set. Expect it to be empty.
TEST(HpackEncodingContextTest, ProcessContextUpdateEmptyReferenceSet) {
  HpackEncodingContext encoding_context;
  test::HpackEncodingContextPeer peer(encoding_context);

  uint32 kEntryCount = 50;

  for (uint32 i = 1; i <= kEntryCount; ++i) {
    uint32 new_index = 0;
    std::vector<uint32> removed_referenced_indices;
    EXPECT_TRUE(
        encoding_context.ProcessIndexedHeader(i,
                                              &new_index,
                                              &removed_referenced_indices));
    EXPECT_EQ(1u, new_index);
    EXPECT_TRUE(removed_referenced_indices.empty());
    EXPECT_EQ(i, encoding_context.GetMutableEntryCount());
  }

  for (uint32 i = 1; i <= kEntryCount; ++i) {
    EXPECT_TRUE(peer.header_table().GetEntry(i).IsReferenced());
  }
  encoding_context.ProcessContextUpdateEmptyReferenceSet();
  for (uint32 i = 1; i <= kEntryCount; ++i) {
    EXPECT_FALSE(peer.header_table().GetEntry(i).IsReferenced());
  }
}

// NOTE: It's too onerous to try to test invalid input to
// ProcessLiteralHeaderWithIncrementalIndexing(); that would require
// making a really large (>4GB of memory) string.

// Try to process a reasonably-sized literal header with incremental
// indexing. It should succeed.
TEST(HpackEncodingContextTest, LiteralHeaderIncrementalIndexing) {
  HpackEncodingContext encoding_context;

  uint32 index = 0;
  std::vector<uint32> removed_referenced_indices;
  EXPECT_TRUE(
      encoding_context.ProcessLiteralHeaderWithIncrementalIndexing(
          "name", "value", &index, &removed_referenced_indices));
  EXPECT_EQ(1u, index);
  EXPECT_TRUE(removed_referenced_indices.empty());
  EXPECT_EQ("name", encoding_context.GetNameAt(1).as_string());
  EXPECT_EQ("value", encoding_context.GetValueAt(1).as_string());
  EXPECT_TRUE(encoding_context.IsReferencedAt(1));
  EXPECT_EQ(1u, encoding_context.GetMutableEntryCount());
}

// Try to process a literal header with incremental indexing that is
// too large for the header table. It should succeed without indexing
// into the table.
TEST(HpackEncodingContextTest, LiteralHeaderIncrementalIndexingDoesNotFit) {
  HpackEncodingContext encoding_context;
  encoding_context.ProcessContextUpdateNewMaximumSize(0);

  uint32 index = 0;
  std::vector<uint32> removed_referenced_indices;
  EXPECT_TRUE(
      encoding_context.ProcessLiteralHeaderWithIncrementalIndexing(
          "name", "value", &index, &removed_referenced_indices));
  EXPECT_EQ(0u, index);
  EXPECT_TRUE(removed_referenced_indices.empty());
  EXPECT_EQ(0u, encoding_context.GetMutableEntryCount());
}

}  // namespace

}  // namespace net
