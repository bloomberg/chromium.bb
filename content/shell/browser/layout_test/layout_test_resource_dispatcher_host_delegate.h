// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "content/shell/browser/shell_resource_dispatcher_host_delegate.h"

namespace content {

class LayoutTestResourceDispatcherHostDelegate
    : public ShellResourceDispatcherHostDelegate {
 public:
  LayoutTestResourceDispatcherHostDelegate();
  ~LayoutTestResourceDispatcherHostDelegate() override;

  // ResourceDispatcherHostDelegate implementation.
  ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info,
      net::URLRequest* request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LayoutTestResourceDispatcherHostDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
