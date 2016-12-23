// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_test_util.h"

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

namespace predictors {

using URLRequestSummary = ResourcePrefetchPredictor::URLRequestSummary;
using PageRequestSummary = ResourcePrefetchPredictor::PageRequestSummary;

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

NavigationID CreateNavigationID(SessionID::id_type tab_id,
                                const std::string& main_frame_url) {
  NavigationID navigation_id;
  navigation_id.tab_id = tab_id;
  navigation_id.main_frame_url = GURL(main_frame_url);
  navigation_id.creation_time = base::TimeTicks::Now();
  return navigation_id;
}

PageRequestSummary CreatePageRequestSummary(
    const std::string& main_frame_url,
    const std::string& initial_url,
    const std::vector<URLRequestSummary>& subresource_requests) {
  GURL main_frame_gurl(main_frame_url);
  PageRequestSummary summary(main_frame_gurl);
  summary.initial_url = GURL(initial_url);
  summary.subresource_requests = subresource_requests;
  return summary;
}

URLRequestSummary CreateURLRequestSummary(SessionID::id_type tab_id,
                                          const std::string& main_frame_url,
                                          const std::string& resource_url,
                                          content::ResourceType resource_type,
                                          net::RequestPriority priority,
                                          const std::string& mime_type,
                                          bool was_cached,
                                          const std::string& redirect_url,
                                          bool has_validators,
                                          bool always_revalidate) {
  URLRequestSummary summary;
  summary.navigation_id = CreateNavigationID(tab_id, main_frame_url);
  summary.resource_url =
      resource_url.empty() ? GURL(main_frame_url) : GURL(resource_url);
  summary.resource_type = resource_type;
  summary.priority = priority;
  summary.mime_type = mime_type;
  summary.was_cached = was_cached;
  if (!redirect_url.empty())
    summary.redirect_url = GURL(redirect_url);
  summary.has_validators = has_validators;
  summary.always_revalidate = always_revalidate;
  return summary;
}

std::ostream& operator<<(std::ostream& os, const PrefetchData& data) {
  os << "[" << data.primary_key() << "," << data.last_visit_time() << "]"
     << std::endl;
  for (const ResourceData& resource : data.resources())
    os << "\t\t" << resource << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const ResourceData& resource) {
  return os << "[" << resource.resource_url() << "," << resource.resource_type()
            << "," << resource.number_of_hits() << ","
            << resource.number_of_misses() << ","
            << resource.consecutive_misses() << ","
            << resource.average_position() << "," << resource.priority() << ","
            << resource.has_validators() << "," << resource.always_revalidate()
            << "]";
}

std::ostream& operator<<(std::ostream& os, const RedirectData& data) {
  os << "[" << data.primary_key() << "," << data.last_visit_time() << "]"
     << std::endl;
  for (const RedirectStat& redirect : data.redirect_endpoints())
    os << "\t\t" << redirect << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const RedirectStat& redirect) {
  return os << "[" << redirect.url() << "," << redirect.number_of_hits() << ","
            << redirect.number_of_misses() << ","
            << redirect.consecutive_misses() << "]";
}

std::ostream& operator<<(std::ostream& os, const PageRequestSummary& summary) {
  os << "[" << summary.main_frame_url << "," << summary.initial_url << "]"
     << std::endl;
  for (const auto& request : summary.subresource_requests)
    os << "\t\t" << request << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const URLRequestSummary& summary) {
  return os << "[" << summary.navigation_id << "," << summary.resource_url
            << "," << summary.resource_type << "," << summary.priority << ","
            << summary.mime_type << "," << summary.was_cached << ","
            << summary.redirect_url << "," << summary.has_validators << ","
            << summary.always_revalidate << "]";
}

std::ostream& operator<<(std::ostream& os, const NavigationID& navigation_id) {
  return os << navigation_id.tab_id << "," << navigation_id.main_frame_url;
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

bool operator==(const PageRequestSummary& lhs, const PageRequestSummary& rhs) {
  return lhs.main_frame_url == rhs.main_frame_url &&
         lhs.initial_url == rhs.initial_url &&
         lhs.subresource_requests == rhs.subresource_requests;
}

bool operator==(const URLRequestSummary& lhs, const URLRequestSummary& rhs) {
  return lhs.navigation_id == rhs.navigation_id &&
         lhs.resource_url == rhs.resource_url &&
         lhs.resource_type == rhs.resource_type &&
         lhs.priority == rhs.priority && lhs.mime_type == rhs.mime_type &&
         lhs.was_cached == rhs.was_cached &&
         lhs.redirect_url == rhs.redirect_url &&
         lhs.has_validators == rhs.has_validators &&
         lhs.always_revalidate == rhs.always_revalidate;
}

}  // namespace predictors
