// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/google/core/browser/google_util.h"

namespace search {

bool IsInstantExtendedAPIEnabled() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  return false;
#else
  return true;
#endif
}

uint64_t EmbeddedSearchPageVersion() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  return 1;
#else
  return 2;
#endif
}

std::string InstantExtendedEnabledParam() {
  return std::string(google_util::kInstantExtendedAPIParam) + "=" +
         base::Uint64ToString(EmbeddedSearchPageVersion()) + "&";
}

std::string ForceInstantResultsParam(bool for_prerender) {
  return (for_prerender || !IsInstantExtendedAPIEnabled()) ? "ion=1&"
                                                           : std::string();
}

}  // namespace search
