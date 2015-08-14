// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/variations/variations_util.h"

#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/variations/fieldtrial_testing_config.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/escape.h"

namespace chrome_variations {

namespace {

std::string EscapeValue(const std::string& value) {
  return net::UnescapeURLComponent(value, net::UnescapeRule::URL_SPECIAL_CHARS);
}

} // namespace

void SetChildProcessLoggingVariationList() {
  std::vector<std::string> experiment_strings;
  variations::GetFieldTrialActiveGroupIdsAsStrings(&experiment_strings);
  crash_keys::SetVariationsList(experiment_strings);
}

bool AssociateParamsFromString(const std::string& varations_string) {
  // Format: Trial1.Group1:k1/v1/k2/v2,Trial2.Group2:k1/v1/k2/v2
  for (const base::StringPiece& experiment_group : base::SplitStringPiece(
           varations_string, ",",
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    std::vector<base::StringPiece> experiment = base::SplitStringPiece(
        experiment_group, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (experiment.size() != 2)
      return false;

    std::vector<std::string> group_parts = base::SplitString(
        experiment[0], ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (group_parts.size() != 2)
      return false;

    std::vector<std::string> key_values = base::SplitString(
        experiment[1], "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (key_values.size() % 2 != 0)
      return false;

    std::map<std::string, std::string> params;
    for (size_t i = 0; i < key_values.size(); i += 2) {
      std::string key = EscapeValue(key_values[i]);
      std::string value = EscapeValue(key_values[i + 1]);
      params[key] = value;
    }
    std::string trial = EscapeValue(group_parts[0]);
    std::string group = EscapeValue(group_parts[1]);
    variations::AssociateVariationParams(trial, group, params);
  }
  return true;
}

void AssociateParamsFromFieldTrialConfig(
    const FieldTrialTestingConfig& config) {
  for (size_t i = 0; i < config.groups_size; ++i) {
    const FieldTrialTestingGroup& group = config.groups[i];
    if (group.params_size != 0) {
      std::map<std::string, std::string> params;
      for (size_t j = 0; j < group.params_size; ++j) {
        const FieldTrialGroupParams& param = group.params[j];
        params[param.key] = param.value;
      }
      variations::AssociateVariationParams(group.study, group.group_name,
                                           params);
    }
    base::FieldTrialList::CreateFieldTrial(group.study, group.group_name);
  }
}

void AssociateDefaultFieldTrialConfig() {
  AssociateParamsFromFieldTrialConfig(kFieldTrialConfig);
}

}  // namespace chrome_variations
