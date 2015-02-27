// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_CLIENT_INFO_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_CLIENT_INFO_H_

#include "content/public/common/request_context_frame_type.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "url/gurl.h"

namespace content {

// This class holds the information related to a service worker window client.
// It is the content/ equivalent of Blink's WebServiceWorkerClientInfo.
// An instance can be created empty or can be filed with the expected
// properties. Except for the client_id, it is preferred to use the constructor
// to fill the properties.
struct ServiceWorkerClientInfo {
  ServiceWorkerClientInfo();
  ServiceWorkerClientInfo(blink::WebPageVisibilityState page_visibility_state,
                          bool is_focused,
                          const GURL& url,
                          RequestContextFrameType frame_type);

  // Returns whether the instance is empty.
  bool IsEmpty() const;

  // Returns whether the instance is valid. A valid instance is not empty and
  // has a valid client_id.
  bool IsValid() const;

  int client_id;
  blink::WebPageVisibilityState page_visibility_state;
  bool is_focused;
  GURL url;
  RequestContextFrameType frame_type;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_CLIENT_INFO_H_
