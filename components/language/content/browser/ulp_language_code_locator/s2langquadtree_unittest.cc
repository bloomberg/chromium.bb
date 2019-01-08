// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/s2cellid/src/s2/s2cellid.h"

namespace {

template <size_t numbits>
S2LangQuadTreeNode GetTree(const std::vector<std::string>& languages,
                           std::bitset<numbits> tree) {
  return S2LangQuadTreeNode::Deserialize(languages, tree);
}

}  // namespace

namespace language {

TEST(S2LangQuadTreeTest, Empty) {
  S2LangQuadTreeNode root;
  const S2CellId cell = S2CellId::FromFace(0);
  const std::string language = root.Get(cell);
  EXPECT_TRUE(language.empty());
}

TEST(S2LangQuadTreeTest, RootIsLeaf_FaceIsPresent) {
  const std::vector<std::string> languages{"fr"};
  const std::bitset<2> tree("11");  // String is in reverse order.
  const S2LangQuadTreeNode root = GetTree(languages, tree);
  const S2CellId cell = S2CellId::FromFace(0);
  const std::string language = root.Get(cell);
  EXPECT_EQ(language, "fr");
}

TEST(S2LangQuadTreeTest, RootIsLeaf_FaceChildGetsFaceLanguage) {
  const std::vector<std::string> languages{"fr"};
  const std::bitset<2> tree("11");  // String is in reverse order.
  const S2LangQuadTreeNode root = GetTree(languages, tree);
  const S2CellId cell = S2CellId::FromFace(0).child(0);
  const std::string language = root.Get(cell);
  EXPECT_EQ(language, "fr");
}

TEST(S2LangQuadTreeTest, RootThenSingleLeaf_LeafIsPresent) {
  const std::vector<std::string> languages{"fr"};
  const std::bitset<9> tree("110101010");  // String is in reverse order.
  const S2LangQuadTreeNode root = GetTree(languages, tree);
  const S2CellId cell = S2CellId::FromFace(0).child(3);
  const std::string language = root.Get(cell);
  EXPECT_EQ(language, "fr");
}

TEST(S2LangQuadTreeTest, RootThenSingleLeaf_ParentIsAbsent) {
  const std::vector<std::string> languages{"fr"};
  const std::bitset<9> tree("110101010");  // String is in reverse order.
  const S2LangQuadTreeNode root = GetTree(languages, tree);
  const S2CellId cell = S2CellId::FromFace(0);
  const std::string language = root.Get(cell);
  LOG(INFO) << language;
  EXPECT_TRUE(language.empty());
}

TEST(S2LangQuadTreeTest, RootThenSingleLeaf_SiblingIsAbsent) {
  const std::vector<std::string> languages{"fr"};
  const std::bitset<9> tree("110101010");  // String is in reverse order.
  const S2LangQuadTreeNode root = GetTree(languages, tree);
  const S2CellId cell = S2CellId::FromFace(0).child(0);
  const std::string language = root.Get(cell);
  EXPECT_TRUE(language.empty());
}

TEST(S2LangQuadTreeTest, RootThenAllLeaves_LeavesArePresent) {
  const std::vector<std::string> languages{"fr", "en"};
  const std::bitset<13> tree("0111011011010");  // String is in reverse order.
  const S2LangQuadTreeNode root = GetTree(languages, tree);
  for (int leaf_index = 0; leaf_index < 3; leaf_index++) {
    const S2CellId cell = S2CellId::FromFace(0).child(leaf_index);
    const std::string language = root.Get(cell);
    EXPECT_EQ(language, "fr");
  }
  const S2CellId cell = S2CellId::FromFace(0).child(3);
  const std::string language = root.Get(cell);
  EXPECT_EQ(language, "en");
}

TEST(S2LangQuadTreeTest, RootThenAllLeaves_ParentIsAbsent) {
  const std::vector<std::string> languages{"fr", "en"};
  const std::bitset<13> tree("0111011011010");  // String is in reverse order.
  const S2LangQuadTreeNode root = GetTree(languages, tree);
  const S2CellId cell = S2CellId::FromFace(0);
  const std::string language = root.Get(cell);
  EXPECT_TRUE(language.empty());
}

}  // namespace language
