// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_CONFIGURATION_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"

namespace base {
struct Feature;
}

namespace feature_engagement_tracker {

// A ComparatorType describes the relationship between two numbers.
enum ComparatorType {
  ANY = 0,  // Will always yield true.
  LESS_THAN = 1,
  GREATER_THAN = 2,
  LESS_THAN_OR_EQUAL = 3,
  GREATER_THAN_OR_EQUAL = 4,
  EQUAL = 5,
  NOT_EQUAL = 6,
};

// A Comparator provides a way of comparing a uint32_t another uint32_t and
// verifying their relationship.
struct Comparator {
 public:
  Comparator();
  Comparator(ComparatorType type, uint32_t value);
  ~Comparator();

  // Returns true if the |v| meets the this criteria based on the current
  // |type| and |value|.
  bool MeetsCriteria(uint32_t v) const;

  ComparatorType type;
  uint32_t value;
};

bool operator==(const Comparator& lhs, const Comparator& rhs);
bool operator<(const Comparator& lhs, const Comparator& rhs);

// A EventConfig contains all the information about how many times
// a particular event should or should not have triggered, for which window
// to search in and for how long to store it.
struct EventConfig {
 public:
  EventConfig();
  EventConfig(const std::string& name,
              Comparator comparator,
              uint32_t window,
              uint32_t storage);
  ~EventConfig();

  // The identifier of the event.
  std::string name;

  // The number of events it is required to find within the search window.
  Comparator comparator;

  // Search for this event within this window.
  uint32_t window;

  // Store client side data related to events for this minimum this long.
  uint32_t storage;
};

bool operator==(const EventConfig& lhs, const EventConfig& rhs);
bool operator!=(const EventConfig& lhs, const EventConfig& rhs);
bool operator<(const EventConfig& lhs, const EventConfig& rhs);

// A FeatureConfig contains all the configuration for a given feature.
struct FeatureConfig {
 public:
  FeatureConfig();
  FeatureConfig(const FeatureConfig& other);
  ~FeatureConfig();

  // Whether the configuration has been successfully parsed.
  bool valid;

  // The configuration for a particular event that will be searched for when
  // counting how many times a particular feature has been used.
  EventConfig used;

  // The configuration for a particular event that will be searched for when
  // counting how many times in-product help has been triggered for a particular
  // feature.
  EventConfig trigger;

  // A set of all event configurations.
  std::set<EventConfig> event_configs;

  // Number of in-product help triggered within this session must fit this
  // comparison.
  Comparator session_rate;

  // Number of days the in-product help has been available must fit this
  // comparison.
  Comparator availability;
};

bool operator==(const FeatureConfig& lhs, const FeatureConfig& rhs);
// A Configuration contains the current set of runtime configurations.
// It is up to each implementation of Configuration to provide a way to
// register features and their configurations.
class Configuration {
 public:
  // Convenience alias for typical implementations of Configuration.
  using ConfigMap = std::map<const base::Feature*, FeatureConfig>;

  virtual ~Configuration() = default;

  // Returns the FeatureConfig for the given |feature|. The |feature| must
  // be registered with the Configuration instance.
  virtual const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const = 0;

 protected:
  Configuration() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Configuration);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_CONFIGURATION_H_
