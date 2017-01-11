// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics_constants_util_win.h"

#include "chrome/installer/util/browser_distribution.h"

namespace {

// Returns the registry path for this product with |suffix| appended to it.
// |suffix| is expected to provide the separator.
base::string16 GetSuffixedRegistryPath(const wchar_t* suffix) {
  BrowserDistribution* chrome_distribution =
      BrowserDistribution::GetDistribution();
  DCHECK(chrome_distribution);

  DCHECK_EQ(L'\\', *suffix);
  DCHECK_NE(L'\\', chrome_distribution->GetRegistryPath().back());

  return chrome_distribution->GetRegistryPath() + suffix;
}

}  // namespace

namespace chrome {

base::string16 GetBrowserExitCodesRegistryPath() {
  return GetSuffixedRegistryPath(L"\\BrowserExitCodes");
}

}  // namespace chrome
