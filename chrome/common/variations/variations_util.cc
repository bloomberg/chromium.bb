// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/variations/variations_util.h"

#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/crash_keys.h"
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
  std::vector<std::string> experiment_groups;
  base::SplitString(varations_string, ',', &experiment_groups);
  for (const auto& experiment_group : experiment_groups) {
    std::vector<std::string> experiment;
    base::SplitString(experiment_group, ':', &experiment);
    if (experiment.size() != 2)
      return false;

    std::vector<std::string> group_parts;
    base::SplitString(experiment[0], '.', &group_parts);
    if (group_parts.size() != 2)
      return false;

    std::vector<std::string> key_values;
    base::SplitString(experiment[1], '/', &key_values);
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

}  // namespace chrome_variations
