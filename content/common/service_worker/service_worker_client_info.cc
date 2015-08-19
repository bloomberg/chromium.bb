// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_client_info.h"

#include "base/logging.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

ServiceWorkerClientInfo::ServiceWorkerClientInfo()
    : page_visibility_state(blink::WebPageVisibilityStateLast),
      is_focused(false),
      frame_type(REQUEST_CONTEXT_FRAME_TYPE_LAST),
      client_type(blink::WebServiceWorkerClientTypeLast),
      last_focus_time(base::TimeTicks()) {}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    blink::WebPageVisibilityState page_visibility_state,
    bool is_focused,
    const GURL& url,
    RequestContextFrameType frame_type,
    base::TimeTicks last_focus_time,
    blink::WebServiceWorkerClientType client_type)
    : page_visibility_state(page_visibility_state),
      is_focused(is_focused),
      url(url),
      frame_type(frame_type),
      client_type(client_type),
      last_focus_time(last_focus_time) {}

bool ServiceWorkerClientInfo::IsEmpty() const {
  return page_visibility_state == blink::WebPageVisibilityStateLast &&
         is_focused == false &&
         url.is_empty() &&
         frame_type == REQUEST_CONTEXT_FRAME_TYPE_LAST &&
         client_type == blink::WebServiceWorkerClientTypeLast;
}

bool ServiceWorkerClientInfo::IsValid() const {
  return !IsEmpty() && !client_uuid.empty();
}

}  // namespace content
