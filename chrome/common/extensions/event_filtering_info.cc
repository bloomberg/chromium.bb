// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/event_filtering_info.h"

#include "base/values.h"
#include "base/json/json_writer.h"

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

}  // namespace extensions
