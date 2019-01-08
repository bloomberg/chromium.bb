// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"

#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"
#include "third_party/s2cellid/src/s2/s2cellid.h"
#include "third_party/s2cellid/src/s2/s2latlng.h"

namespace language {

UlpLanguageCodeLocator::UlpLanguageCodeLocator(
    std::unique_ptr<S2LangQuadTreeNode> root)
    : root_(std::move(root)) {}

UlpLanguageCodeLocator::~UlpLanguageCodeLocator() {}

std::vector<std::string> UlpLanguageCodeLocator::GetLanguageCode(
    double latitude,
    double longitude) const {
  S2CellId cell(S2LatLng::FromDegrees(latitude, longitude));
  std::vector<std::string> languages;
  const std::string language = root_.get()->Get(cell);
  if (!language.empty())
    languages.push_back(language);
  return languages;
}

}  // namespace language
