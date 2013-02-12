// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_filtering_info.h"

#include "base/json/json_writer.h"
#include "base/values.h"

namespace extensions {

EventFilteringInfo::EventFilteringInfo()
    : has_url_(false) {
}

EventFilteringInfo::~EventFilteringInfo() {
}

void EventFilteringInfo::SetURL(const GURL& url) {
  url_ = url;
  has_url_ = true;
}

std::string EventFilteringInfo::AsJSONString() const {
  std::string result;
  base::DictionaryValue value;
  if (has_url_)
    value.SetString("url", url_.spec());

  base::JSONWriter::Write(&value, &result);
  return result;
}

scoped_ptr<base::Value> EventFilteringInfo::AsValue() const {
  if (IsEmpty())
    return scoped_ptr<base::Value>(base::Value::CreateNullValue());

  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  if (has_url_)
    result->SetString("url", url_.spec());
  return result.PassAs<base::Value>();
}

bool EventFilteringInfo::IsEmpty() const {
  return !has_url_;
}

}  // namespace extensions
