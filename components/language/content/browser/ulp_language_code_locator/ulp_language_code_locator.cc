// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"

#include <memory>

#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"
#include "third_party/s2cellid/src/s2/s2cellid.h"
#include "third_party/s2cellid/src/s2/s2latlng.h"

namespace language {

struct UlpLanguageCodeLocator::CellLanguagePair {
  S2CellId cell = S2CellId::None();
  std::string language = "";
};

UlpLanguageCodeLocator::UlpLanguageCodeLocator(
    std::vector<std::unique_ptr<S2LangQuadTreeNode>>&& roots) {
  roots_ = std::move(roots);
  cache_ = std::vector<CellLanguagePair>(roots_.size());
}

UlpLanguageCodeLocator::~UlpLanguageCodeLocator() {}

std::vector<std::string> UlpLanguageCodeLocator::GetLanguageCodes(
    double latitude,
    double longitude) const {
  S2CellId cell(S2LatLng::FromDegrees(latitude, longitude));
  std::vector<std::string> languages;
  for (size_t index = 0; index < roots_.size(); index++) {
    CellLanguagePair cached = cache_[index];
    std::string language;
    if (cached.cell.is_valid() && cached.cell.contains(cell)) {
      language = cached.language;
    } else {
      const auto& root = roots_[index];
      int level;
      language = root.get()->Get(cell, &level);
      if (level != -1) {
        //|cell|.parent(|level|) is the ancestor S2Cell of |cell| for which
        // there's a matching language in the tree.
        cache_[index].cell = cell.parent(level);
        cache_[index].language = language;
      }
    }
    if (!language.empty())
      languages.push_back(language);
  }
  return languages;
}

}  // namespace language
