// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/test/fake_search_provider.h"

namespace ios {

FakeSearchProvider::FakeSearchProvider() {
}

FakeSearchProvider::~FakeSearchProvider() {
}

bool FakeSearchProvider::IsQueryExtractionEnabled() {
  return false;
}

std::string FakeSearchProvider::InstantExtendedEnabledParam(bool for_search) {
  return std::string();
}

std::string FakeSearchProvider::ForceInstantResultsParam(bool for_prerender) {
  return std::string();
}

int FakeSearchProvider::OmniboxStartMargin() {
  return 0;
}

std::string FakeSearchProvider::GoogleImageSearchSource() {
  return std::string();
}

}  // namespace ios
