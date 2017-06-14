// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_matcher.h"

#include <utility>

#include "base/callback.h"
#include "extensions/common/event_filtering_info.h"

namespace {
const char kUrlFiltersKey[] = "url";
const char kWindowTypesKey[] = "windowTypes";
}

namespace extensions {

const char kEventFilterServiceTypeKey[] = "serviceType";

EventMatcher::EventMatcher(std::unique_ptr<base::DictionaryValue> filter,
                           int routing_id)
    : filter_(std::move(filter)), routing_id_(routing_id) {}

EventMatcher::~EventMatcher() {
}

bool EventMatcher::MatchNonURLCriteria(
    const EventFilteringInfo& event_info) const {
  if (event_info.instance_id) {
    return *event_info.instance_id == GetInstanceID();
  }

  if (event_info.window_type) {
    int window_type_count = GetWindowTypeCount();
    for (int i = 0; i < window_type_count; i++) {
      std::string window_type;
      if (GetWindowType(i, &window_type) &&
          window_type == *event_info.window_type) {
        return true;
      }
    }
    return false;
  }

  if (event_info.window_exposed_by_default) {
    // An event with a |window_exposed_by_default| set is only
    // relevant to the listener if no window type filter is set.
    if (HasWindowTypes())
      return false;
    return *event_info.window_exposed_by_default;
  }

  const std::string& service_type_filter = GetServiceTypeFilter();
  return service_type_filter.empty() ||
         (event_info.service_type &&
          service_type_filter == *event_info.service_type);
}

int EventMatcher::GetURLFilterCount() const {
  base::ListValue* url_filters = nullptr;
  if (filter_->GetList(kUrlFiltersKey, &url_filters))
    return url_filters->GetSize();
  return 0;
}

bool EventMatcher::GetURLFilter(int i, base::DictionaryValue** url_filter_out) {
  base::ListValue* url_filters = nullptr;
  if (filter_->GetList(kUrlFiltersKey, &url_filters)) {
    return url_filters->GetDictionary(i, url_filter_out);
  }
  return false;
}

bool EventMatcher::HasURLFilters() const {
  return GetURLFilterCount() != 0;
}

std::string EventMatcher::GetServiceTypeFilter() const {
  std::string service_type_filter;
  filter_->GetStringASCII(kEventFilterServiceTypeKey, &service_type_filter);
  return service_type_filter;
}

int EventMatcher::GetInstanceID() const {
  int instance_id = 0;
  filter_->GetInteger("instanceId", &instance_id);
  return instance_id;
}

int EventMatcher::GetWindowTypeCount() const {
  base::ListValue* window_type_filters = nullptr;
  if (filter_->GetList(kWindowTypesKey, &window_type_filters))
    return window_type_filters->GetSize();
  return 0;
}

bool EventMatcher::GetWindowType(int i, std::string* window_type_out) const {
  base::ListValue* window_types = nullptr;
  if (filter_->GetList(kWindowTypesKey, &window_types)) {
    return window_types->GetString(i, window_type_out);
  }
  return false;
}

bool EventMatcher::HasWindowTypes() const {
  return GetWindowTypeCount() != 0;
}

int EventMatcher::GetRoutingID() const {
  return routing_id_;
}

}  // namespace extensions
