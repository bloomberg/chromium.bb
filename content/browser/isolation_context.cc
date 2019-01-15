// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/isolation_context.h"

namespace content {

IsolationContext::IsolationContext() = default;

IsolationContext::IsolationContext(BrowsingInstanceId browsing_instance_id)
    : browsing_instance_id_(browsing_instance_id) {}

}  // namespace content
