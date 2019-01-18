// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"

#include <memory>

#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"
#include "third_party/s2cellid/src/s2/s2cellid.h"
#include "third_party/s2cellid/src/s2/s2latlng.h"

namespace language {

UlpLanguageCodeLocator::UlpLanguageCodeLocator(
    std::vector<std::unique_ptr<S2LangQuadTreeNode>>&& roots) {
  roots_ = std::move(roots);
}

UlpLanguageCodeLocator::~UlpLanguageCodeLocator() {}

std::vector<std::string> UlpLanguageCodeLocator::GetLanguageCodes(
    double latitude,
    double longitude) const {
  S2CellId cell(S2LatLng::FromDegrees(latitude, longitude));
  std::vector<std::string> languages;
  for (const auto& root : roots_) {
    const std::string language = root.get()->Get(cell);
    if (!language.empty())
      languages.push_back(language);
  }
  return languages;
}

}  // namespace language
