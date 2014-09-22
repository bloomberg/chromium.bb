// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_event_router_delegate.h"

namespace extensions {

WebRequestEventRouterDelegate::WebRequestEventRouterDelegate() {
}

WebRequestEventRouterDelegate::~WebRequestEventRouterDelegate() {
}

bool WebRequestEventRouterDelegate::OnGetMatchingListenersImplCheck(
    int tab_id, int window_id, net::URLRequest* request) {
  return false;
}

}  // namespace extensions
