// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_DISCOVERY_PROVIDER_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_DISCOVERY_PROVIDER_H_

#include "components/devtools_discovery/devtools_discovery_manager.h"

class ChromeDevToolsDiscoveryProvider :
    public devtools_discovery::DevToolsDiscoveryManager::Provider {
 public:
  // Installs provider to devtools_discovery.
  static void Install();

  ~ChromeDevToolsDiscoveryProvider() override;

  // devtools_discovery::DevToolsDiscoveryManager::Provider implementation.
  devtools_discovery::DevToolsTargetDescriptor::List GetDescriptors() override;

 private:
  ChromeDevToolsDiscoveryProvider();

  DISALLOW_COPY_AND_ASSIGN(ChromeDevToolsDiscoveryProvider);
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_DISCOVERY_PROVIDER_H_
