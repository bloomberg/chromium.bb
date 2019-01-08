// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"

#include <bitset>
#include <string>
#include <vector>

#include "base/logging.h"
#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/s2cellid/src/s2/s2cellid.h"
#include "third_party/s2cellid/src/s2/s2latlng.h"

namespace language {

std::unique_ptr<S2LangQuadTreeNode> GetTestTree() {
  const std::vector<std::string> languages{"fr", "en"};
  const std::bitset<13> tree("0111011011010");  // String is in reverse order.
  return std::make_unique<S2LangQuadTreeNode>(
      S2LangQuadTreeNode::Deserialize(languages, tree));
}

TEST(UlpLanguageCodeLocatorTest, QuadrantMatchLanguages) {
  const UlpLanguageCodeLocator locator(GetTestTree());
  const S2CellId face = S2CellId::FromFace(0);
  for (int position = 0; position < 3; position++) {
    const S2LatLng latlng = face.child(position).ToLatLng();
    const std::vector<std::string> languages =
        locator.GetLanguageCode(latlng.lat().degrees(), latlng.lng().degrees());
    EXPECT_THAT(languages, ::testing::UnorderedElementsAreArray({"fr"}));
  }
  const S2LatLng latlng = face.child(3).ToLatLng();
  const std::vector<std::string> languages =
      locator.GetLanguageCode(latlng.lat().degrees(), latlng.lng().degrees());
  EXPECT_THAT(languages, ::testing::UnorderedElementsAreArray({"en"}));
}

}  // namespace language
