// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/previews_experiments.h"

#include <map>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "components/variations/variations_associated_data.h"

namespace {

// The group of client-side previews experiments.
const char kClientSidePreviewsFieldTrial[] = "ClientSidePreviews";

const char kEnabled[] = "Enabled";

// Allow offline pages to show for prohibitively slow networks.
const char kOfflinePagesSlowNetwork[] = "show_offline_pages";

// The string that corresponds to enabled for the variation param experiments.
const char kExperimentEnabled[] = "true";

}  // namespace

namespace previews {

bool IsIncludedInClientSidePreviewsExperimentsFieldTrial() {
  // By convention, an experiment in the client-side previews study enables use
  // of at least one client-side previews optimization if its name begins with
  // "Enabled."
  return base::StartsWith(
      base::FieldTrialList::FindFullName(kClientSidePreviewsFieldTrial),
      kEnabled, base::CompareCase::SENSITIVE);
}

bool IsOfflinePreviewsEnabled() {
  if (!IsIncludedInClientSidePreviewsExperimentsFieldTrial())
    return false;
  std::map<std::string, std::string> experiment_params;
  if (!variations::GetVariationParams(kClientSidePreviewsFieldTrial,
                                      &experiment_params)) {
    return false;
  }
  std::map<std::string, std::string>::const_iterator it =
      experiment_params.find(kOfflinePagesSlowNetwork);
  return it != experiment_params.end() && it->second == kExperimentEnabled;
}

bool EnableOfflinePreviewsForTesting() {
  std::map<std::string, std::string> params;
  params[kOfflinePagesSlowNetwork] = kExperimentEnabled;
  return variations::AssociateVariationParams(kClientSidePreviewsFieldTrial,
                                              kEnabled, params) &&
         base::FieldTrialList::CreateFieldTrial(kClientSidePreviewsFieldTrial,
                                                kEnabled);
}

}  // namespace previews
