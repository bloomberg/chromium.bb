// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_TARGET_DESCRIPTOR_H_
#define COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_TARGET_DESCRIPTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/public/browser/devtools_target.h"
#include "url/gurl.h"

namespace content {
class DevToolsAgentHost;
}

namespace devtools_discovery {

// DevToolsTargetDescriptor provides information about devtools target
// and can be used to manipulate the target and query its details.
// Instantiation and discovery of DevToolsTargetDescriptor instances
// is the responsibility of DevToolsDiscoveryManager.
// TODO(dgozman): remove content::DevToolsTarget once every embedder migrates
// to this descriptor.
class DevToolsTargetDescriptor : public content::DevToolsTarget {
 public:
  using List = std::vector<DevToolsTargetDescriptor*>;
  ~DevToolsTargetDescriptor() override {}
};

}  // namespace devtools_discovery

#endif  // COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_TARGET_DESCRIPTOR_H_
