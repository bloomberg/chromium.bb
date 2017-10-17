// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/language_code_locator.h"

#include "base/strings/string_split.h"
#include "third_party/s2cellid/src/s2/s2cellid.h"
#include "third_party/s2cellid/src/s2/s2latlng.h"

namespace language {
namespace internal {
extern std::vector<std::pair<uint64_t, std::string>>
GenerateDistrictLanguageMapping();
}  // namespace internal

LanguageCodeLocator::LanguageCodeLocator()
    : district_languages_(internal::GenerateDistrictLanguageMapping()) {}

LanguageCodeLocator::~LanguageCodeLocator() {}

std::vector<std::string> LanguageCodeLocator::GetLanguageCode(
    double latitude,
    double longitude) const {
  S2CellId current_cell(S2LatLng::FromDegrees(latitude, longitude));
  while (current_cell.level() > 0) {
    auto search = district_languages_.find(current_cell.id());
    if (search != district_languages_.end()) {
      return base::SplitString(search->second, ";", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_ALL);
    }
    current_cell = current_cell.parent();
  }
  return {};
}

}  // namespace language
