// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/event_filtering_info.h"

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

}  // namespace extensions
