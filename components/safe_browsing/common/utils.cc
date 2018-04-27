// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/common/utils.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "crypto/sha2.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

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

void LogNoUserActionResourceLoadingDelay(base::TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SB2.NoUserActionResourceLoadingDelay", time);
}

ChromeUserPopulation::ProfileManagementStatus GetProfileManagementStatus() {
#if defined(OS_WIN)
  if (base::win::IsEnterpriseManaged())
    return ChromeUserPopulation::ENTERPRISE_MANAGED;
  else
    return ChromeUserPopulation::NOT_MANAGED;
#else
  // TODO(crbug/796332): Add check for OS_CHROMEOS also.
  return ChromeUserPopulation::UNAVAILABLE;
#endif  // #if defined(OS_WIN)
}

}  // namespace safe_browsing
