// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/variations/variations_util.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/variations/fieldtrial_testing_config.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/escape.h"

namespace chrome_variations {

namespace {

std::string EscapeValue(const std::string& value) {
  return net::UnescapeURLComponent(
      value, net::UnescapeRule::PATH_SEPARATORS |
                 net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
}

void AssociateParamsFromGroup(const std::string& trial_name,
                              const FieldTrialTestingGroup& group,
                              base::FeatureList* feature_list) {
  if (group.params_size != 0) {
    std::map<std::string, std::string> params;
    for (size_t i = 0; i < group.params_size; ++i) {
      const FieldTrialTestingGroupParams& param = group.params[i];
      params[param.key] = param.value;
    }
    variations::AssociateVariationParams(trial_name, group.name, params);
  }
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(trial_name, group.name);

  if (!trial) {
    DLOG(WARNING) << "Field trial config trial skipped: " << trial_name
                  << "." << group.name
                  << " (it is overridden from chrome://flags)";
    return;
  }

  for (size_t i = 0; i < group.enable_features_size; ++i) {
    feature_list->RegisterFieldTrialOverride(
        group.enable_features[i], base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        trial);
  }
  for (size_t i = 0; i < group.disable_features_size; ++i) {
    feature_list->RegisterFieldTrialOverride(
        group.disable_features[i],
        base::FeatureList::OVERRIDE_DISABLE_FEATURE, trial);
  }
}

} // namespace

bool AssociateParamsFromString(const std::string& varations_string) {
  // Format: Trial1.Group1:k1/v1/k2/v2,Trial2.Group2:k1/v1/k2/v2
  std::set<std::pair<std::string, std::string>> trial_groups;
  for (const base::StringPiece& experiment_group : base::SplitStringPiece(
           varations_string, ",",
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    std::vector<base::StringPiece> experiment = base::SplitStringPiece(
        experiment_group, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (experiment.size() != 2) {
      DLOG(ERROR) << "Experiment and params should be separated by ':'";
      return false;
    }

    std::vector<std::string> group_parts = base::SplitString(
        experiment[0], ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (group_parts.size() != 2) {
      DLOG(ERROR) << "Trial and group name should be separated by '.'";
      return false;
    }

    std::vector<std::string> key_values = base::SplitString(
        experiment[1], "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (key_values.size() % 2 != 0) {
      DLOG(ERROR) << "Param name and param value should be separated by '/'";
      return false;
    }
    std::string trial = EscapeValue(group_parts[0]);
    std::string group = EscapeValue(group_parts[1]);
    auto trial_group = std::make_pair(trial, group);
    if (trial_groups.find(trial_group) != trial_groups.end()) {
      DLOG(ERROR) << base::StringPrintf(
          "A (trial, group) pair listed more than once. (%s, %s)",
          trial.c_str(), group.c_str());
      return false;
    }
    trial_groups.insert(trial_group);
    std::map<std::string, std::string> params;
    for (size_t i = 0; i < key_values.size(); i += 2) {
      std::string key = EscapeValue(key_values[i]);
      std::string value = EscapeValue(key_values[i + 1]);
      params[key] = value;
    }
    variations::AssociateVariationParams(trial, group, params);
  }
  return true;
}

void AssociateParamsFromFieldTrialConfig(const FieldTrialTestingConfig& config,
                                         base::FeatureList* feature_list) {
  for (size_t i = 0; i < config.trials_size; ++i) {
    const FieldTrialTestingTrial& trial = config.trials[i];
    if (trial.groups_size > 0) {
      AssociateParamsFromGroup(trial.name, trial.groups[0], feature_list);
    } else {
      DLOG(ERROR) << "Unexpected empty trial: " << trial.name;
    }
  }
}

void AssociateDefaultFieldTrialConfig(base::FeatureList* feature_list) {
  AssociateParamsFromFieldTrialConfig(kFieldTrialConfig, feature_list);
}

}  // namespace chrome_variations
