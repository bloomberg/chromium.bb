// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DEV_TOOLS_DISCOVERY_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DEV_TOOLS_DISCOVERY_PROVIDER_H_

#include "base/macros.h"
#include "components/devtools_discovery/devtools_discovery_manager.h"

namespace android_webview {

class AwDevToolsDiscoveryProvider :
    public devtools_discovery::DevToolsDiscoveryManager::Provider {
 public:
  // Installs provider to devtools_discovery.
  static void Install();

  ~AwDevToolsDiscoveryProvider() override;

  // devtools_discovery::DevToolsDiscoveryManager::Provider implementation.
  devtools_discovery::DevToolsTargetDescriptor::List GetDescriptors() override;

 private:
  AwDevToolsDiscoveryProvider();

  DISALLOW_COPY_AND_ASSIGN(AwDevToolsDiscoveryProvider);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DEV_TOOLS_DISCOVERY_PROVIDER_H_
