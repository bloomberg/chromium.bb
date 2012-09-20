// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/async_dns_field_trial.h"

#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "chrome/common/chrome_version_info.h"

namespace chrome_browser_net {

bool ConfigureAsyncDnsFieldTrial() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  // There is no DnsConfigService on those platforms so disable the field trial.
  return false;
#endif
  const base::FieldTrial::Probability kAsyncDnsDivisor = 100;
  base::FieldTrial::Probability enabled_probability = 0;

#if defined(OS_CHROMEOS)
  if (chrome::VersionInfo::GetChannel() <= chrome::VersionInfo::CHANNEL_DEV)
    enabled_probability = 50;
#endif

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "AsyncDns", kAsyncDnsDivisor, "disabled", 2012, 10, 30, NULL));

  int enabled_group = trial->AppendGroup("enabled", enabled_probability);
  return trial->group() == enabled_group;
}

}  // namespace chrome_browser_net
