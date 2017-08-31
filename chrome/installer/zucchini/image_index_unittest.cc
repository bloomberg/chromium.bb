// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/image_index.h"

#include <stddef.h>

#include <numeric>
#include <vector>

#include "base/test/gtest_util.h"
#include "chrome/installer/zucchini/image_utils.h"
#include "chrome/installer/zucchini/test_disassembler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

class ImageIndexTest : public testing::Test {
 protected:
  ImageIndexTest()
      : buffer_(20),
        image_index_(ConstBufferView(buffer_.data(), buffer_.size())) {
    std::iota(buffer_.begin(), buffer_.end(), 0);
  }

  void InitializeWithDefaultTestData() {
    TestDisassembler disasm({2, TypeTag(0), PoolTag(0)},
                            {{1, 0}, {8, 1}, {10, 2}},
                            {4, TypeTag(1), PoolTag(0)}, {{3, 3}},
                            {3, TypeTag(2), PoolTag(1)}, {{12, 4}, {17, 5}});
    EXPECT_TRUE(image_index_.Initialize(&disasm));
  }

  std::vector<uint8_t> buffer_;
  ImageIndex image_index_;
};

TEST_F(ImageIndexTest, TypeAndPool) {
  TestDisassembler disasm({2, TypeTag(0), PoolTag(0)}, {},
                          {4, TypeTag(1), PoolTag(0)}, {},
                          {3, TypeTag(2), PoolTag(1)}, {});
  EXPECT_TRUE(image_index_.Initialize(&disasm));

  EXPECT_EQ(3U, image_index_.TypeCount());
  EXPECT_EQ(2U, image_index_.PoolCount());

  EXPECT_EQ(PoolTag(0), image_index_.GetPoolTag(TypeTag(0)));
  EXPECT_EQ(PoolTag(0), image_index_.GetPoolTag(TypeTag(1)));
  EXPECT_EQ(PoolTag(1), image_index_.GetPoolTag(TypeTag(2)));

  EXPECT_EQ(0U, image_index_.LabelBound(PoolTag(0)));
  EXPECT_EQ(0U, image_index_.LabelBound(PoolTag(1)));
}

TEST_F(ImageIndexTest, InvalidInsertReferences1) {
  // Overlap within the same group.
  TestDisassembler disasm({2, TypeTag(0), PoolTag(0)}, {{1, 0}, {2, 0}},
                          {4, TypeTag(1), PoolTag(0)}, {},
                          {3, TypeTag(2), PoolTag(1)}, {});
  EXPECT_FALSE(image_index_.Initialize(&disasm));
}

TEST_F(ImageIndexTest, InvalidInsertReferences2) {
  TestDisassembler disasm({2, TypeTag(0), PoolTag(0)},
                          {{1, 0}, {8, 1}, {10, 2}},
                          {4, TypeTag(1), PoolTag(0)}, {{3, 3}},
                          {3, TypeTag(2), PoolTag(1)}, {{11, 0}});

  // Overlap across different readers.
  EXPECT_FALSE(image_index_.Initialize(&disasm));
}

TEST_F(ImageIndexTest, LookupType) {
  InitializeWithDefaultTestData();

  std::vector<int> expected = {
      -1,            // raw
      0,  0,         // ref 0
      1,  1,  1, 1,  // ref 1
      -1,            // raw
      0,  0,         // ref 0
      0,  0,         // ref 0
      2,  2,  2,     // ref 2
      -1, -1,        // raw
      2,  2,  2,     // ref 2
  };

  for (offset_t i = 0; i < image_index_.size(); ++i)
    EXPECT_EQ(TypeTag(expected[i]), image_index_.LookupType(i));
}

TEST_F(ImageIndexTest, IsToken) {
  InitializeWithDefaultTestData();

  std::vector<bool> expected = {
      1,           // raw
      1, 0,        // ref 0
      1, 0, 0, 0,  // ref 1
      1,           // raw
      1, 0,        // ref 0
      1, 0,        // ref 0
      1, 0, 0,     // ref 2
      1, 1,        // raw
      1, 0, 0,     // ref 2
  };

  for (offset_t i = 0; i < image_index_.size(); ++i)
    EXPECT_EQ(expected[i], image_index_.IsToken(i));
}

TEST_F(ImageIndexTest, IsReference) {
  InitializeWithDefaultTestData();

  std::vector<bool> expected = {
      0,           // raw
      1, 1,        // ref 0
      1, 1, 1, 1,  // ref 1
      0,           // raw
      1, 1,        // ref 0
      1, 1,        // ref 0
      1, 1, 1,     // ref 2
      0, 0,        // raw
      1, 1, 1,     // ref 2
  };

  for (offset_t i = 0; i < image_index_.size(); ++i)
    EXPECT_EQ(expected[i], image_index_.IsReference(i));
}

TEST_F(ImageIndexTest, FindReference) {
  InitializeWithDefaultTestData();

  EXPECT_DCHECK_DEATH(image_index_.FindReference(TypeTag(0), 0));
  EXPECT_EQ(Reference({1, 0}), image_index_.FindReference(TypeTag(0), 1));
  EXPECT_EQ(Reference({1, 0}), image_index_.FindReference(TypeTag(0), 2));
  EXPECT_DCHECK_DEATH(image_index_.FindReference(TypeTag(0), 3));
  EXPECT_DCHECK_DEATH(image_index_.FindReference(TypeTag(1), 1));
  EXPECT_DCHECK_DEATH(image_index_.FindReference(TypeTag(1), 2));
  EXPECT_EQ(Reference({10, 2}), image_index_.FindReference(TypeTag(0), 10));
  EXPECT_EQ(Reference({10, 2}), image_index_.FindReference(TypeTag(0), 11));
  EXPECT_DCHECK_DEATH(image_index_.FindReference(TypeTag(0), 12));
}

TEST_F(ImageIndexTest, LabelTargets) {
  InitializeWithDefaultTestData();

  OrderedLabelManager label_manager0;
  label_manager0.InsertOffsets({0, 2, 3, 4});
  image_index_.LabelTargets(PoolTag(0), label_manager0);
  EXPECT_EQ(4U, image_index_.LabelBound(PoolTag(0)));
  EXPECT_EQ(0U, image_index_.LabelBound(PoolTag(1)));

  EXPECT_EQ(
      std::vector<Reference>({{1, MarkIndex(0)}, {8, 1}, {10, MarkIndex(1)}}),
      image_index_.GetReferences(TypeTag(0)));
  EXPECT_EQ(std::vector<Reference>({{3, MarkIndex(2)}}),
            image_index_.GetReferences(TypeTag(1)));
  EXPECT_EQ(std::vector<Reference>({{12, 4}, {17, 5}}),
            image_index_.GetReferences(TypeTag(2)));

  OrderedLabelManager label_manager1;
  label_manager1.InsertOffsets({5});
  image_index_.LabelTargets(PoolTag(1), label_manager1);
  EXPECT_EQ(4U, image_index_.LabelBound(PoolTag(0)));
  EXPECT_EQ(1U, image_index_.LabelBound(PoolTag(1)));

  EXPECT_EQ(std::vector<Reference>({{12, 4}, {17, MarkIndex(0)}}),
            image_index_.GetReferences(TypeTag(2)));

  image_index_.UnlabelTargets(PoolTag(0), label_manager0);
  EXPECT_EQ(0U, image_index_.LabelBound(PoolTag(0)));
  EXPECT_EQ(1U, image_index_.LabelBound(PoolTag(1)));

  EXPECT_EQ(std::vector<Reference>({{1, 0}, {8, 1}, {10, 2}}),
            image_index_.GetReferences(TypeTag(0)));
  EXPECT_EQ(std::vector<Reference>({{3, 3}}),
            image_index_.GetReferences(TypeTag(1)));
  EXPECT_EQ(std::vector<Reference>({{12, 4}, {17, MarkIndex(0)}}),
            image_index_.GetReferences(TypeTag(2)));
}

TEST_F(ImageIndexTest, LabelAssociatedTargets) {
  InitializeWithDefaultTestData();

  OrderedLabelManager label_manager;
  label_manager.InsertOffsets({0, 1, 2, 3, 4});

  UnorderedLabelManager reference_label_manager;
  reference_label_manager.Init({0, kUnusedIndex, 2});

  image_index_.LabelAssociatedTargets(PoolTag(0), label_manager,
                                      reference_label_manager);
  EXPECT_EQ(5U, image_index_.LabelBound(PoolTag(0)));
  EXPECT_EQ(0U, image_index_.LabelBound(PoolTag(1)));
  EXPECT_EQ(
      std::vector<Reference>({{1, MarkIndex(0)}, {8, 1}, {10, MarkIndex(2)}}),
      image_index_.GetReferences(TypeTag(0)));
  EXPECT_EQ(std::vector<Reference>({{3, 3}}),
            image_index_.GetReferences(TypeTag(1)));
  EXPECT_EQ(std::vector<Reference>({{12, 4}, {17, 5}}),
            image_index_.GetReferences(TypeTag(2)));
}

}  // namespace zucchini
