// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/site_isolation_policy.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"

namespace content {

// static
bool SiteIsolationPolicy::UseDedicatedProcessesForAllSites() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSitePerProcess) ||
         base::FeatureList::IsEnabled(features::kSitePerProcess);
}

// static
SiteIsolationPolicy::CrossSiteDocumentBlockingEnabledState
SiteIsolationPolicy::IsCrossSiteDocumentBlockingEnabled() {
  if (base::FeatureList::IsEnabled(
          ::features::kCrossSiteDocumentBlockingAlways))
    return XSDB_ENABLED_UNCONDITIONALLY;

  if (base::FeatureList::IsEnabled(
          ::features::kCrossSiteDocumentBlockingIfIsolating)) {
    return XSDB_ENABLED_IF_ISOLATED;
  }

  return XSDB_DISABLED;
}

// static
bool SiteIsolationPolicy::IsTopDocumentIsolationEnabled() {
  // --site-per-process trumps --top-document-isolation.
  if (UseDedicatedProcessesForAllSites())
    return false;

  return base::FeatureList::IsEnabled(::features::kTopDocumentIsolation);
}

// static
bool SiteIsolationPolicy::AreIsolatedOriginsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kIsolateOrigins) ||
         base::FeatureList::IsEnabled(features::kIsolateOrigins);
}

// static
std::vector<url::Origin> SiteIsolationPolicy::GetIsolatedOrigins() {
  std::string cmdline_arg =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIsolateOrigins);
  if (!cmdline_arg.empty()) {
    std::vector<url::Origin> cmdline_origins =
        ParseIsolatedOrigins(cmdline_arg);
    UMA_HISTOGRAM_COUNTS_1000("SiteIsolation.IsolateOrigins.Size",
                              cmdline_origins.size());
    return cmdline_origins;
  }

  if (base::FeatureList::IsEnabled(features::kIsolateOrigins)) {
    std::string field_trial_arg = base::GetFieldTrialParamValueByFeature(
        features::kIsolateOrigins,
        features::kIsolateOriginsFieldTrialParamName);
    return ParseIsolatedOrigins(field_trial_arg);
  }

  return std::vector<url::Origin>();
}

// static
std::vector<url::Origin> SiteIsolationPolicy::ParseIsolatedOrigins(
    base::StringPiece arg) {
  std::vector<base::StringPiece> origin_strings = base::SplitStringPiece(
      arg, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::vector<url::Origin> origins;
  origins.reserve(origin_strings.size());
  for (const base::StringPiece& origin_string : origin_strings) {
    url::Origin origin = url::Origin::Create(GURL(origin_string));
    if (!origin.unique())
      origins.push_back(origin);
  }
  return origins;
}

// static
void SiteIsolationPolicy::RecordSiteIsolationFlagUsage() {
  // For --site-per-process and --isolate-origins, include flags specified on
  // command-line, in chrome://flags, and via enterprise policy (i.e., include
  // switches::kSitePerProcess and switches::kIsolateOrigins).  Exclude these
  // modes being set through field trials (i.e., exclude
  // features::kSitePerProcess and features::IsolateOrigins).
  UMA_HISTOGRAM_BOOLEAN("SiteIsolation.Flags.IsolateOrigins",
                        base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kIsolateOrigins));

  UMA_HISTOGRAM_BOOLEAN("SiteIsolation.Flags.SitePerProcess",
                        base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kSitePerProcess));
}

}  // namespace content
