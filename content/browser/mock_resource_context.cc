// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mock_resource_context.h"

#include "base/lazy_instance.h"
#include "net/url_request/url_request_test_util.h"

namespace content {

static base::LazyInstance<MockResourceContext>
    g_mock_resource_context(base::LINKER_INITIALIZED);

const ResourceContext& MockResourceContext::GetInstance() {
  return g_mock_resource_context.Get();
}

MockResourceContext::MockResourceContext()
    : test_request_context_(new TestURLRequestContext) {
  set_request_context(test_request_context_);
}
MockResourceContext::~MockResourceContext() {}

void MockResourceContext::EnsureInitialized() const {}

}  // namespace content
