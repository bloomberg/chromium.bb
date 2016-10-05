// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_test_util.h"

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

namespace predictors {

void InitializeResourceData(ResourceData* resource,
                            const std::string& resource_url,
                            content::ResourceType resource_type,
                            int number_of_hits,
                            int number_of_misses,
                            int consecutive_misses,
                            double average_position,
                            net::RequestPriority priority,
                            bool has_validators,
                            bool always_revalidate) {
  resource->set_resource_url(resource_url);
  resource->set_resource_type(
      static_cast<ResourceData::ResourceType>(resource_type));
  resource->set_number_of_hits(number_of_hits);
  resource->set_number_of_misses(number_of_misses);
  resource->set_consecutive_misses(consecutive_misses);
  resource->set_average_position(average_position);
  resource->set_priority(static_cast<ResourceData::Priority>(priority));
  resource->set_has_validators(has_validators);
  resource->set_always_revalidate(always_revalidate);
}

void InitializeRedirectStat(RedirectStat* redirect,
                            const std::string& url,
                            int number_of_hits,
                            int number_of_misses,
                            int consecutive_misses) {
  redirect->set_url(url);
  redirect->set_number_of_hits(number_of_hits);
  redirect->set_number_of_misses(number_of_misses);
  redirect->set_consecutive_misses(consecutive_misses);
}

PrefetchData CreatePrefetchData(const std::string& primary_key,
                                uint64_t last_visit_time) {
  PrefetchData data;
  data.set_primary_key(primary_key);
  data.set_last_visit_time(last_visit_time);
  return data;
}

RedirectData CreateRedirectData(const std::string& primary_key,
                                uint64_t last_visit_time) {
  RedirectData data;
  data.set_primary_key(primary_key);
  data.set_last_visit_time(last_visit_time);
  return data;
}

void PrintTo(const PrefetchData& data, ::std::ostream* os) {
  *os << "[" << data.primary_key() << "," << data.last_visit_time() << "]\n";
  for (const ResourceData& resource : data.resources()) {
    *os << "\t\t";
    PrintTo(resource, os);
    *os << "\n";
  }
}

void PrintTo(const ResourceData& resource, ::std::ostream* os) {
  *os << "[" << resource.resource_url() << "," << resource.resource_type()
      << "," << resource.number_of_hits() << "," << resource.number_of_misses()
      << "," << resource.consecutive_misses() << ","
      << resource.average_position() << "," << resource.priority() << ","
      << resource.has_validators() << "," << resource.always_revalidate()
      << "]";
}

void PrintTo(const RedirectData& data, ::std::ostream* os) {
  *os << "[" << data.primary_key() << "," << data.last_visit_time() << "]\n";
  for (const RedirectStat& redirect : data.redirect_endpoints()) {
    *os << "\t\t";
    PrintTo(redirect, os);
    *os << "\n";
  }
}

void PrintTo(const RedirectStat& redirect, ::std::ostream* os) {
  *os << "[" << redirect.url() << "," << redirect.number_of_hits() << ","
      << redirect.number_of_misses() << "," << redirect.consecutive_misses()
      << "]";
}

bool operator==(const PrefetchData& lhs, const PrefetchData& rhs) {
  bool equal = lhs.primary_key() == rhs.primary_key() &&
               lhs.resources_size() == rhs.resources_size();

  if (!equal)
    return false;

  for (int i = 0; i < lhs.resources_size(); ++i)
    equal = equal && lhs.resources(i) == rhs.resources(i);

  return equal;
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

bool operator==(const RedirectData& lhs, const RedirectData& rhs) {
  bool equal = lhs.primary_key() == rhs.primary_key() &&
               lhs.redirect_endpoints_size() == rhs.redirect_endpoints_size();

  if (!equal)
    return false;

  for (int i = 0; i < lhs.redirect_endpoints_size(); ++i)
    equal = equal && lhs.redirect_endpoints(i) == rhs.redirect_endpoints(i);

  return equal;
}

bool operator==(const RedirectStat& lhs, const RedirectStat& rhs) {
  return lhs.url() == rhs.url() &&
         lhs.number_of_hits() == rhs.number_of_hits() &&
         lhs.number_of_misses() == rhs.number_of_misses() &&
         lhs.consecutive_misses() == rhs.consecutive_misses();
}

}  // namespace predictors
