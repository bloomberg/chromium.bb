// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"

#include <bitset>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/s2cellid/src/s2/s2cellid.h"
#include "third_party/s2cellid/src/s2/s2latlng.h"

namespace language {

std::vector<std::unique_ptr<S2LangQuadTreeNode>> GetTestTrees() {
  const std::vector<std::string> languages_rank0{"fr", "en"};
  // |tree_rank0| is a two level quadtree with the second level being all leaves
  // with language indices 0, 0, 0, 1.
  const std::bitset<13> tree_rank0(
      "0111011011010");  // String is in reverse order.

  const std::vector<std::string> languages_rank1{"en", "de"};
  // |tree_rank1| is a two level quadtree with the second level being all leaves
  // with language indices 1, 0, 0, 0.
  const std::bitset<13> tree_rank1(
      "1011011010110");  // String is in reverse order.

  std::vector<std::unique_ptr<S2LangQuadTreeNode>> roots;
  roots.reserve(2);
  roots.push_back(std::make_unique<S2LangQuadTreeNode>(
      S2LangQuadTreeNode::Deserialize(languages_rank0, tree_rank0)));
  roots.push_back(std::make_unique<S2LangQuadTreeNode>(
      S2LangQuadTreeNode::Deserialize(languages_rank1, tree_rank1)));
  return roots;
}

void ExpectLatLngHasLanguages(const UlpLanguageCodeLocator& locator,
                              S2CellId cell,
                              std::vector<std::string> languages_expected) {
  const S2LatLng latlng = cell.ToLatLng();
  const std::vector<std::string> languages =
      locator.GetLanguageCodes(latlng.lat().degrees(), latlng.lng().degrees());
  EXPECT_THAT(languages, ::testing::ElementsAreArray(languages_expected));
}

TEST(UlpLanguageCodeLocatorTest, TreeLeaves) {
  std::vector<std::unique_ptr<S2LangQuadTreeNode>> roots = GetTestTrees();
  const UlpLanguageCodeLocator locator(std::move(roots));
  const S2CellId face = S2CellId::FromFace(0);

  ExpectLatLngHasLanguages(locator, face.child(0), {"fr", "de"});
  ExpectLatLngHasLanguages(locator, face.child(1), {"fr", "en"});
  ExpectLatLngHasLanguages(locator, face.child(2), {"fr", "en"});
  ExpectLatLngHasLanguages(locator, face.child(3), {"en", "en"});
}

TEST(UlpLanguageCodeLocatorTest, Idempotence) {
  std::vector<std::unique_ptr<S2LangQuadTreeNode>> roots = GetTestTrees();
  const UlpLanguageCodeLocator locator(std::move(roots));
  const S2CellId face = S2CellId::FromFace(0);

  ExpectLatLngHasLanguages(locator, face.child(0), {"fr", "de"});
  ExpectLatLngHasLanguages(locator, face.child(0), {"fr", "de"});

  ExpectLatLngHasLanguages(locator, face.child(3), {"en", "en"});
  ExpectLatLngHasLanguages(locator, face.child(3), {"en", "en"});
}

TEST(UlpLanguageCodeLocatorTest, TreeLeafDescendants) {
  std::vector<std::unique_ptr<S2LangQuadTreeNode>> roots = GetTestTrees();
  const UlpLanguageCodeLocator locator(std::move(roots));
  const S2CellId cell = S2CellId::FromFace(0).child(0);

  ExpectLatLngHasLanguages(locator, cell, {"fr", "de"});

  const int depth = 2;
  // Check that the 4**|depth| descendants of |child| map to the same language.
  const int level = cell.level() + depth;
  for (S2CellId descendant = cell.child_begin(level);
       descendant != cell.child_end(level); descendant = descendant.next())
    ExpectLatLngHasLanguages(locator, descendant, {"fr", "de"});
}

}  // namespace language
