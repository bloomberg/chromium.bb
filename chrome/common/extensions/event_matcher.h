// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EVENT_MATCHER_H_
#define CHROME_COMMON_EXTENSIONS_EVENT_MATCHER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace extensions {

class EventFilteringInfo;

// Matches EventFilteringInfos against a set of criteria. This is intended to
// be used by EventFilter which performs efficient URL matching across
// potentially many EventMatchers itself. This is why this class only exposes
// MatchNonURLCriteria() - URL matching is handled by EventFilter.
class EventMatcher {
 public:
  EventMatcher();
  ~EventMatcher();

  // Returns true if |event_info| satisfies this matcher's criteria, not taking
  // into consideration any URL criteria.
  bool MatchNonURLCriteria(const EventFilteringInfo& event_info) const;

  void set_url_filters(scoped_ptr<base::ListValue> url_filters) {
    url_filters_ = url_filters.Pass();
  }

  // Returns NULL if no url_filters have been specified.
  base::ListValue* url_filters() const {
    return url_filters_.get();
  }

  bool has_url_filters() const {
    return url_filters_.get() && !url_filters_->empty();
  }

 private:
  scoped_ptr<base::ListValue> url_filters_;

  DISALLOW_COPY_AND_ASSIGN(EventMatcher);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_EVENT_MATCHER_H_
