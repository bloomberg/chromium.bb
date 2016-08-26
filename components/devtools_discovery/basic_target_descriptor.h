// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_DISCOVERY_BASIC_TARGET_DESCRIPTOR_H_
#define COMPONENTS_DEVTOOLS_DISCOVERY_BASIC_TARGET_DESCRIPTOR_H_

#include "components/devtools_discovery/devtools_target_descriptor.h"

namespace devtools_discovery {

class BasicTargetDescriptor : public DevToolsTargetDescriptor {
 public:
  explicit BasicTargetDescriptor(
      scoped_refptr<content::DevToolsAgentHost> agent_host);
  ~BasicTargetDescriptor() override;

  scoped_refptr<content::DevToolsAgentHost> GetAgentHost() const override;

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
};

}  // namespace devtools_discovery

#endif  // COMPONENTS_DEVTOOLS_DISCOVERY_BASIC_TARGET_DESCRIPTOR_H_
