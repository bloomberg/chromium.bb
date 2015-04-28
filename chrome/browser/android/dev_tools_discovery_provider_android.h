// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DEV_TOOLS_DISCOVERY_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_DEV_TOOLS_DISCOVERY_PROVIDER_ANDROID_H_

#include "components/devtools_discovery/devtools_discovery_manager.h"

class DevToolsDiscoveryProviderAndroid :
    public devtools_discovery::DevToolsDiscoveryManager::Provider {
 public:
  // Installs provider to devtools_discovery.
  static void Install();

  ~DevToolsDiscoveryProviderAndroid() override;

  // devtools_discovery::DevToolsDiscoveryManager::Provider implementation.
  devtools_discovery::DevToolsTargetDescriptor::List GetDescriptors() override;

 private:
  DevToolsDiscoveryProviderAndroid();

  DISALLOW_COPY_AND_ASSIGN(DevToolsDiscoveryProviderAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_DEV_TOOLS_DISCOVERY_PROVIDER_ANDROID_H_
