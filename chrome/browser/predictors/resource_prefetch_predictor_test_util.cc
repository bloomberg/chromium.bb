// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_test_util.h"

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

namespace predictors {

using PrefetchData = ResourcePrefetchPredictorTables::PrefetchData;

ResourceData CreateResourceData(const std::string& resource_url,
                                content::ResourceType resource_type,
                                int number_of_hits,
                                int number_of_misses,
                                int consecutive_misses,
                                double average_position,
                                net::RequestPriority priority,
                                bool has_validators,
                                bool always_revalidate) {
  ResourceData resource;
  resource.set_resource_url(resource_url);
  resource.set_resource_type(
      static_cast<ResourceData::ResourceType>(resource_type));
  resource.set_number_of_hits(number_of_hits);
  resource.set_number_of_misses(number_of_misses);
  resource.set_consecutive_misses(consecutive_misses);
  resource.set_average_position(average_position);
  resource.set_priority(static_cast<ResourceData::Priority>(priority));
  resource.set_has_validators(has_validators);
  resource.set_always_revalidate(always_revalidate);
  return resource;
}

void PrintTo(const ResourceData& resource, ::std::ostream* os) {
  *os << "[" << resource.resource_url() << "," << resource.resource_type()
      << "," << resource.number_of_hits() << "," << resource.number_of_misses()
      << "," << resource.consecutive_misses() << ","
      << resource.average_position() << "," << resource.priority() << ","
      << resource.has_validators() << "," << resource.always_revalidate()
      << "]";
}

void PrintTo(const PrefetchData& data, ::std::ostream* os) {
  *os << "[" << data.key_type << "," << data.primary_key << ","
      << data.last_visit.ToInternalValue() << "]\n";
  for (const ResourceData& resource : data.resources) {
    *os << "\t\t";
    PrintTo(resource, os);
    *os << "\n";
  }
}

bool operator==(const ResourceData& lhs, const ResourceData& rhs) {
  return lhs.resource_url() == rhs.resource_url() &&
         lhs.resource_type() == rhs.resource_type() &&
         lhs.number_of_hits() == rhs.number_of_hits() &&
         lhs.number_of_misses() == rhs.number_of_misses() &&
         lhs.consecutive_misses() == rhs.consecutive_misses() &&
         lhs.average_position() == rhs.average_position() &&
         lhs.priority() == rhs.priority() &&
         lhs.has_validators() == rhs.has_validators() &&
         lhs.always_revalidate() == rhs.always_revalidate();
}

bool operator==(const PrefetchData& lhs, const PrefetchData& rhs) {
  bool equal = lhs.key_type == rhs.key_type &&
               lhs.primary_key == rhs.primary_key &&
               lhs.resources.size() == rhs.resources.size();

  if (!equal)
    return false;

  for (size_t i = 0; i < lhs.resources.size(); ++i)
    equal = equal && lhs.resources[i] == rhs.resources[i];

  return equal;
}

}  // namespace predictors
