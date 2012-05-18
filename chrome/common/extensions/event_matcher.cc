// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/event_matcher.h"
#include "chrome/common/extensions/event_filtering_info.h"

namespace extensions {

EventMatcher::EventMatcher() {
}

EventMatcher::~EventMatcher() {
}

bool EventMatcher::MatchNonURLCriteria(
    const EventFilteringInfo& event_info) const {
  // There is currently no criteria apart from URL criteria.
  return true;
}

}  // namespace extensions
