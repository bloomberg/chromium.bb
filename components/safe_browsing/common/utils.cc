// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/common/utils.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/sha2.h"

namespace safe_browsing {

std::string ShortURLForReporting(const GURL& url) {
  std::string spec(url.spec());
  if (url.SchemeIs(url::kDataScheme)) {
    size_t comma_pos = spec.find(',');
    if (comma_pos != std::string::npos && comma_pos != spec.size() - 1) {
      std::string hash_value = crypto::SHA256HashString(spec);
      spec.erase(comma_pos + 1);
      spec += base::HexEncode(hash_value.data(), hash_value.size());
    }
  }
  return spec;
}

void LogDelay(base::TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SB2.Delay", time);
}

void LogNoUserActionResourceLoadingDelay(base::TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SB2.NoUserActionResourceLoadingDelay", time);
}

}  // namespace safe_browsing
