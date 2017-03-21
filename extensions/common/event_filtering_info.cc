// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_filtering_info.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace extensions {

namespace {

const char kInstanceId[] = "instanceId";
const char kServiceType[] = "serviceType";
const char kWindowType[] = "windowType";
const char kWindowExposedByDefault[] = "windowExposedByDefault";

}

EventFilteringInfo::EventFilteringInfo()
    : has_url_(false),
      has_instance_id_(false),
      instance_id_(0),
      has_window_type_(false),
      has_window_exposed_by_default_(false) {}

EventFilteringInfo::EventFilteringInfo(const base::DictionaryValue& dict)
    : EventFilteringInfo() {
  std::string url;
  if (dict.GetString("url", &url)) {
    GURL maybe_url(url);
    if (maybe_url.is_valid()) {
      has_url_ = true;
      url_.Swap(&maybe_url);
    }
  }

  has_instance_id_ = dict.GetInteger(kInstanceId, &instance_id_);
  dict.GetString(kServiceType, &service_type_);
  has_window_type_ = dict.GetString(kWindowType, &window_type_);
  has_window_exposed_by_default_ =
      dict.GetBoolean(kWindowExposedByDefault, &window_exposed_by_default_);
}

EventFilteringInfo::EventFilteringInfo(const EventFilteringInfo& other) =
    default;

EventFilteringInfo::~EventFilteringInfo() {
}

void EventFilteringInfo::SetWindowType(const std::string& window_type) {
  window_type_ = window_type;
  has_window_type_ = true;
}

void EventFilteringInfo::SetWindowExposedByDefault(const bool exposed) {
  window_exposed_by_default_ = exposed;
  has_window_exposed_by_default_ = true;
}

void EventFilteringInfo::SetURL(const GURL& url) {
  url_ = url;
  has_url_ = true;
}

void EventFilteringInfo::SetInstanceID(int instance_id) {
  instance_id_ = instance_id;
  has_instance_id_ = true;
}

std::unique_ptr<base::DictionaryValue> EventFilteringInfo::AsValue() const {
  auto result = base::MakeUnique<base::DictionaryValue>();
  if (has_url_)
    result->SetString("url", url_.spec());

  if (has_instance_id_)
    result->SetInteger(kInstanceId, instance_id_);

  if (!service_type_.empty())
    result->SetString(kServiceType, service_type_);

  if (has_window_type_)
    result->SetString(kWindowType, window_type_);

  if (has_window_exposed_by_default_)
    result->SetBoolean(kWindowExposedByDefault, window_exposed_by_default_);

  return result;
}

}  // namespace extensions
