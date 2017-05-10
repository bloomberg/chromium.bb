// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/configuration.h"

#include <string>

#include "base/logging.h"

namespace feature_engagement_tracker {

Comparator::Comparator() : type(ANY), value(0) {}

Comparator::Comparator(ComparatorType type, uint32_t value)
    : type(type), value(value) {}

Comparator::~Comparator() = default;

bool Comparator::MeetsCriteria(uint32_t v) const {
  switch (type) {
    case ANY:
      return true;
    case LESS_THAN:
      return v < value;
    case GREATER_THAN:
      return v > value;
    case LESS_THAN_OR_EQUAL:
      return v <= value;
    case GREATER_THAN_OR_EQUAL:
      return v >= value;
    case EQUAL:
      return v == value;
    case NOT_EQUAL:
      return v != value;
    default:
      // All cases should be covered.
      NOTREACHED();
      return false;
  }
}

EventConfig::EventConfig() : window(0), storage(0) {}

EventConfig::EventConfig(const std::string& name,
                         Comparator comparator,
                         uint32_t window,
                         uint32_t storage)
    : name(name), comparator(comparator), window(window), storage(storage) {}

EventConfig::~EventConfig() = default;

FeatureConfig::FeatureConfig() : valid(false) {}

FeatureConfig::FeatureConfig(const FeatureConfig& other) = default;

FeatureConfig::~FeatureConfig() = default;

bool operator==(const Comparator& lhs, const Comparator& rhs) {
  return std::tie(lhs.type, lhs.value) == std::tie(rhs.type, rhs.value);
}

bool operator<(const Comparator& lhs, const Comparator& rhs) {
  return std::tie(lhs.type, lhs.value) < std::tie(rhs.type, rhs.value);
}

bool operator==(const EventConfig& lhs, const EventConfig& rhs) {
  return std::tie(lhs.name, lhs.comparator, lhs.window, lhs.storage) ==
         std::tie(rhs.name, rhs.comparator, rhs.window, rhs.storage);
}

bool operator!=(const EventConfig& lhs, const EventConfig& rhs) {
  return !(lhs == rhs);
}

bool operator<(const EventConfig& lhs, const EventConfig& rhs) {
  return std::tie(lhs.name, lhs.comparator, lhs.window, lhs.storage) <
         std::tie(rhs.name, rhs.comparator, rhs.window, rhs.storage);
}

bool operator==(const FeatureConfig& lhs, const FeatureConfig& rhs) {
  return std::tie(lhs.valid, lhs.used, lhs.trigger, lhs.event_configs,
                  lhs.session_rate, lhs.availability) ==
         std::tie(rhs.valid, rhs.used, rhs.trigger, rhs.event_configs,
                  rhs.session_rate, rhs.availability);
}

}  // namespace feature_engagement_tracker
