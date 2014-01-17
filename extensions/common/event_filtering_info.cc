// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_filtering_info.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace extensions {

EventFilteringInfo::EventFilteringInfo()
    : has_url_(false),
      has_instance_id_(false),
      instance_id_(0) {
}

EventFilteringInfo::~EventFilteringInfo() {
}

void EventFilteringInfo::SetURL(const GURL& url) {
  url_ = url;
  has_url_ = true;
}

void EventFilteringInfo::SetInstanceID(int instance_id) {
  instance_id_ = instance_id;
  has_instance_id_ = true;
}

scoped_ptr<base::Value> EventFilteringInfo::AsValue() const {
  if (IsEmpty())
    return scoped_ptr<base::Value>(base::Value::CreateNullValue());

  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  if (has_url_)
    result->SetString("url", url_.spec());

  if (has_instance_id_)
    result->SetInteger("instanceId", instance_id_);

  if (!service_type_.empty())
    result->SetString("serviceType", service_type_);

  return result.PassAs<base::Value>();
}

bool EventFilteringInfo::IsEmpty() const {
  return !has_url_ && service_type_.empty() && !has_instance_id_;
}

}  // namespace extensions
