// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_resource_dispatcher_host_delegate.h"

#include "base/command_line.h"
#include "content/shell/common/shell_switches.h"

namespace content {

LayoutTestResourceDispatcherHostDelegate::
    LayoutTestResourceDispatcherHostDelegate() {
}

LayoutTestResourceDispatcherHostDelegate::
    ~LayoutTestResourceDispatcherHostDelegate() {
}

ResourceDispatcherHostLoginDelegate*
LayoutTestResourceDispatcherHostDelegate::CreateLoginDelegate(
    net::AuthChallengeInfo* auth_info, net::URLRequest* request) {
  return NULL;
}

}  // namespace content
