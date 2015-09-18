// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/navigation_entry.h"

namespace web_view {

NavigationEntry::NavigationEntry(mojo::URLRequestPtr original_request)
    : url_request_(original_request.Pass()) {}

NavigationEntry::~NavigationEntry() {}

mojo::URLRequestPtr NavigationEntry::BuildURLRequest() const {
  return url_request_.Clone();
}

}  // namespace web_view
