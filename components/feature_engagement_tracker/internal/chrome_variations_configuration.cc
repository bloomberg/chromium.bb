// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/chrome_variations_configuration.h"

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/feature_engagement_tracker/internal/configuration.h"
#include "components/feature_engagement_tracker/public/feature_list.h"

namespace {

const char kComparatorTypeAny[] = "any";
const char kComparatorTypeLessThan[] = "<";
const char kComparatorTypeGreaterThan[] = ">";
const char kComparatorTypeLessThanOrEqual[] = "<=";
const char kComparatorTypeGreaterThanOrEqual[] = ">=";
const char kComparatorTypeEqual[] = "==";
const char kComparatorTypeNotEqual[] = "!=";

const char kEventConfigUsedKey[] = "event_used";
const char kEventConfigTriggerKey[] = "event_trigger";
const char kEventConfigKeyPrefix[] = "event_";
const char kSessionRateKey[] = "session_rate";
const char kAvailabilityKey[] = "availability";

const char kEventConfigDataNameKey[] = "name";
const char kEventConfigDataComparatorKey[] = "comparator";
const char kEventConfigDataWindowKey[] = "window";
const char kEventConfigDataStorageKey[] = "storage";

}  // namespace

namespace feature_engagement_tracker {

namespace {

bool ParseComparatorSubstring(base::StringPiece definition,
                              Comparator* comparator,
                              ComparatorType type,
                              uint32_t type_len) {
  base::StringPiece number_string =
      base::TrimWhitespaceASCII(definition.substr(type_len), base::TRIM_ALL);
  uint32_t value;
  if (!base::StringToUint(number_string, &value))
    return false;

  comparator->type = type;
  comparator->value = value;
  return true;
}

bool ParseComparator(base::StringPiece definition, Comparator* comparator) {
  if (base::LowerCaseEqualsASCII(definition, kComparatorTypeAny)) {
    comparator->type = ANY;
    comparator->value = 0;
    return true;
  }

  if (base::StartsWith(definition, kComparatorTypeLessThanOrEqual,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator, LESS_THAN_OR_EQUAL,
                                    2);
  }

  if (base::StartsWith(definition, kComparatorTypeGreaterThanOrEqual,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator,
                                    GREATER_THAN_OR_EQUAL, 2);
  }

  if (base::StartsWith(definition, kComparatorTypeEqual,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator, EQUAL, 2);
  }

  if (base::StartsWith(definition, kComparatorTypeNotEqual,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator, NOT_EQUAL, 2);
  }

  if (base::StartsWith(definition, kComparatorTypeLessThan,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator, LESS_THAN, 1);
  }

  if (base::StartsWith(definition, kComparatorTypeGreaterThan,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ParseComparatorSubstring(definition, comparator, GREATER_THAN, 1);
  }

  return false;
}

bool ParseEventConfig(base::StringPiece definition, EventConfig* event_config) {
  // Support definitions with at least 4 tokens.
  auto tokens = base::SplitStringPiece(definition, ";", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
  if (tokens.size() < 4) {
    *event_config = EventConfig();
    return false;
  }

  // Parse tokens in any order.
  bool has_name = false;
  bool has_comparator = false;
  bool has_window = false;
  bool has_storage = false;
  for (const auto& token : tokens) {
    auto pair = base::SplitStringPiece(token, ":", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
    if (pair.size() != 2) {
      *event_config = EventConfig();
      return false;
    }

    const base::StringPiece& key = pair[0];
    const base::StringPiece& value = pair[1];
    // TODO(nyquist): Ensure that key matches regex /^[a-zA-Z0-9-_]+$/.

    if (base::LowerCaseEqualsASCII(key, kEventConfigDataNameKey)) {
      if (has_name) {
        *event_config = EventConfig();
        return false;
      }
      has_name = true;

      event_config->name = value.as_string();
    } else if (base::LowerCaseEqualsASCII(key, kEventConfigDataComparatorKey)) {
      if (has_comparator) {
        *event_config = EventConfig();
        return false;
      }
      has_comparator = true;

      Comparator comparator;
      if (!ParseComparator(value, &comparator)) {
        *event_config = EventConfig();
        return false;
      }

      event_config->comparator = comparator;
    } else if (base::LowerCaseEqualsASCII(key, kEventConfigDataWindowKey)) {
      if (has_window) {
        *event_config = EventConfig();
        return false;
      }
      has_window = true;

      uint32_t parsed_value;
      if (!base::StringToUint(value, &parsed_value)) {
        *event_config = EventConfig();
        return false;
      }

      event_config->window = parsed_value;
    } else if (base::LowerCaseEqualsASCII(key, kEventConfigDataStorageKey)) {
      if (has_storage) {
        *event_config = EventConfig();
        return false;
      }
      has_storage = true;

      uint32_t parsed_value;
      if (!base::StringToUint(value, &parsed_value)) {
        *event_config = EventConfig();
        return false;
      }

      event_config->storage = parsed_value;
    }
  }

  return has_name && has_comparator && has_window && has_storage;
}

}  // namespace

ChromeVariationsConfiguration::ChromeVariationsConfiguration() = default;

ChromeVariationsConfiguration::~ChromeVariationsConfiguration() = default;

void ChromeVariationsConfiguration::ParseFeatureConfigs(
    FeatureVector features) {
  for (auto* feature : features) {
    ParseFeatureConfig(feature);
  }
}

void ChromeVariationsConfiguration::ParseFeatureConfig(
    const base::Feature* feature) {
  DCHECK(feature);
  DCHECK(configs_.find(feature) == configs_.end());

  // Initially all new configurations are considered invalid.
  FeatureConfig& config = configs_[feature];
  config.valid = false;
  uint32_t parse_errors = 0;

  std::map<std::string, std::string> params;
  bool result = base::GetFieldTrialParamsByFeature(*feature, &params);
  if (!result)
    return;

  for (const auto& it : params) {
    const std::string& key = it.first;
    if (key == kEventConfigUsedKey) {
      EventConfig event_config;
      if (!ParseEventConfig(params[key], &event_config)) {
        ++parse_errors;
        continue;
      }
      config.used = event_config;
    } else if (key == kEventConfigTriggerKey) {
      EventConfig event_config;
      if (!ParseEventConfig(params[key], &event_config)) {
        ++parse_errors;
        continue;
      }
      config.trigger = event_config;
    } else if (key == kSessionRateKey) {
      Comparator comparator;
      if (!ParseComparator(params[key], &comparator)) {
        ++parse_errors;
        continue;
      }
      config.session_rate = comparator;
    } else if (key == kAvailabilityKey) {
      Comparator comparator;
      if (!ParseComparator(params[key], &comparator)) {
        ++parse_errors;
        continue;
      }
      config.availability = comparator;
    } else if (base::StartsWith(key, kEventConfigKeyPrefix,
                                base::CompareCase::INSENSITIVE_ASCII)) {
      EventConfig event_config;
      if (!ParseEventConfig(params[key], &event_config)) {
        ++parse_errors;
        continue;
      }
      config.event_configs.insert(event_config);
    } else {
      DVLOG(1) << "Ignoring unknown key when parsing config for feature "
               << feature->name << ": " << key;
    }
  }

  // The |used| and |trigger| members are required, so should not be the
  // default values.
  config.valid = config.used != EventConfig() &&
                 config.trigger != EventConfig() && parse_errors == 0;
}

const FeatureConfig& ChromeVariationsConfiguration::GetFeatureConfig(
    const base::Feature& feature) const {
  auto it = configs_.find(&feature);
  DCHECK(it != configs_.end());
  return it->second;
}

}  // namespace feature_engagement_tracker
