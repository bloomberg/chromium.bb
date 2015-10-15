// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/navigation_entry.h"

namespace web_view {

NavigationEntry::NavigationEntry(mojo::URLRequestPtr original_request)
    : url_request_(original_request.Pass()) {
  if (url_request_.originating_time().is_null())
    url_request_.set_originating_time(base::TimeTicks::Now());
}

NavigationEntry::NavigationEntry(const GURL& raw_url)
    : url_request_(raw_url) {}

NavigationEntry::~NavigationEntry() {}

mojo::URLRequestPtr NavigationEntry::BuildURLRequest(
    bool update_originating_time) {
  if (update_originating_time)
    url_request_.set_originating_time(base::TimeTicks::Now());
  return url_request_.Clone();
}

}  // namespace web_view
