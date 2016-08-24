// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_TARGET_DESCRIPTOR_H_
#define COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_TARGET_DESCRIPTOR_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace content {
class DevToolsAgentHost;
}

namespace devtools_discovery {

// DevToolsTargetDescriptor provides information about devtools target
// and can be used to manipulate the target and query its details.
// Instantiation and discovery of DevToolsTargetDescriptor instances
// is the responsibility of DevToolsDiscoveryManager.
class DevToolsTargetDescriptor {
 public:
  using List = std::vector<DevToolsTargetDescriptor*>;
  virtual ~DevToolsTargetDescriptor() {}

  // Returns the agent host for this target.
  virtual scoped_refptr<content::DevToolsAgentHost> GetAgentHost() const = 0;
};

}  // namespace devtools_discovery

#endif  // COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_TARGET_DESCRIPTOR_H_
