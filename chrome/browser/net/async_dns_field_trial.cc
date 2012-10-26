// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/async_dns_field_trial.h"

#include "base/metrics/field_trial.h"
#include "base/string_util.h"
#include "build/build_config.h"

namespace chrome_browser_net {

bool ConfigureAsyncDnsFieldTrial() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  // There is no DnsConfigService on those platforms so disable the field trial.
  return false;
#endif
  // Configure the AsyncDns field trial as follows:
  // groups AsyncDnsA and AsyncDnsB: return true,
  // groups SystemDnsA and SystemDnsB: return false,
  // otherwise (trial absent): return false.
  return StartsWithASCII(base::FieldTrialList::FindFullName("AsyncDns"),
                         "AsyncDns", false /* case_sensitive */);
}

}  // namespace chrome_browser_net
