// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_DISCOVERY_MANAGER_H_
#define COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_DISCOVERY_MANAGER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "components/devtools_discovery/devtools_target_descriptor.h"

namespace devtools_discovery {

class DevToolsDiscoveryManager {
 public:
  class Provider {
   public:
    virtual ~Provider() {}
    virtual DevToolsTargetDescriptor::List GetDescriptors() = 0;
  };

  // Returns single instance of this class. The instance is destroyed on the
  // browser main loop exit so this method MUST NOT be called after that point.
  static DevToolsDiscoveryManager* GetInstance();

  void AddProvider(scoped_ptr<Provider> provider);

  DevToolsTargetDescriptor::List GetDescriptors();

 private:
  friend struct DefaultSingletonTraits<DevToolsDiscoveryManager>;

  DevToolsDiscoveryManager();
  ~DevToolsDiscoveryManager();
  DevToolsTargetDescriptor::List GetDescriptorsFromProviders();

  std::vector<Provider*> providers_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsDiscoveryManager);
};

}  // namespace devtools_discovery

#endif  // COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_DISCOVERY_MANAGER_H_
