// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_resource_context.h"

#include "net/url_request/url_request_context.h"

namespace content {

MockResourceContext::MockResourceContext()
  : test_request_context_(NULL) {
}

MockResourceContext::MockResourceContext(
    net::URLRequestContext* test_request_context)
  : test_request_context_(test_request_context),
    mic_allowed_(false),
    camera_allowed_(false) {
}

MockResourceContext::~MockResourceContext() {}

net::HostResolver* MockResourceContext::GetHostResolver()  {
  return NULL;
}

net::URLRequestContext* MockResourceContext::GetRequestContext()  {
  CHECK(test_request_context_);
  return test_request_context_;
}

bool MockResourceContext::AllowMicAccess(const GURL& origin)  {
  return mic_allowed_;
}

bool MockResourceContext::AllowCameraAccess(const GURL& origin)  {
  return camera_allowed_;
}

}  // namespace content
