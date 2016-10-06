// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_service_manager_context.h"

#include "content/browser/service_manager/service_manager_context.h"

namespace content {

TestServiceManagerContext::TestServiceManagerContext()
    : context_(new ServiceManagerContext) {}

TestServiceManagerContext::~TestServiceManagerContext() {}

}  // namespace content
