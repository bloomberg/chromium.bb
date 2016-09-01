// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/async_dns_field_trial.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

namespace chrome_browser_net {

namespace {

// Source of the default value of the async DNS pref; used in histograms.
enum PrefDefaultSource {
  PLATFORM,
  FIELD_TRIAL,
  HARD_CODED_DEFAULT,
  MAX_PREF_DEFAULT_SOURCE
};

// Source of the actual value of the async DNS pref; used in histograms.
enum PrefSource {
  MANAGED_PREF,
  SUPERVISED_PREF,
  EXTENSION_PREF,
  COMMAND_LINE_PREF,
  USER_PREF,
  RECOMMENDED_PREF,
  DEFAULT_PREF,
  UNKNOWN_PREF,
  MAX_PREF_SOURCE
};

void HistogramPrefDefaultSource(PrefDefaultSource source, bool enabled) {
  if (enabled) {
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.PrefDefaultSource_Enabled",
                              source,
                              MAX_PREF_DEFAULT_SOURCE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.PrefDefaultSource_Disabled",
                              source,
                              MAX_PREF_DEFAULT_SOURCE);
  }
}

void HistogramPrefSource(PrefSource source, bool enabled) {
  if (enabled) {
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.PrefSource_Enabled",
                              source,
                              MAX_PREF_SOURCE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.PrefSource_Disabled",
                              source,
                              MAX_PREF_SOURCE);
  }
}

}  // namespace

bool ConfigureAsyncDnsFieldTrial() {
#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
  const bool kDefault = true;
#else
  const bool kDefault = false;
#endif

  // Configure the AsyncDns field trial as follows:
  // groups AsyncDnsA and AsyncDnsB: return true,
  // groups SystemDnsA and SystemDnsB: return false,
  // otherwise (trial absent): return default.
  std::string group_name = base::FieldTrialList::FindFullName("AsyncDns");
  if (!group_name.empty()) {
    const bool enabled = base::StartsWith(group_name, "AsyncDns",
                                          base::CompareCase::INSENSITIVE_ASCII);
    HistogramPrefDefaultSource(FIELD_TRIAL, enabled);
    return enabled;
  }
  HistogramPrefDefaultSource(HARD_CODED_DEFAULT, kDefault);
  return kDefault;
}

void LogAsyncDnsPrefSource(const PrefService::Preference* pref) {
  CHECK(pref);

  const base::Value* value = pref->GetValue();
  CHECK(value);

  bool enabled;
  bool got_value = value->GetAsBoolean(&enabled);
  CHECK(got_value);

  if (pref->IsManaged())
    HistogramPrefSource(MANAGED_PREF, enabled);
  else if (pref->IsExtensionControlled())
    HistogramPrefSource(EXTENSION_PREF, enabled);
  else if (pref->IsUserControlled())
    HistogramPrefSource(USER_PREF, enabled);
  else if (pref->IsDefaultValue())
    HistogramPrefSource(DEFAULT_PREF, enabled);
  else
    HistogramPrefSource(UNKNOWN_PREF, enabled);
}

}  // namespace chrome_browser_net
