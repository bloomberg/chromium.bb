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

  static const char kTypePage[];
  static const char kTypeServiceWorker[];
  static const char kTypeSharedWorker[];
  static const char kTypeOther[];

  // DevToolsTargetDescriptor implementation.
  std::string GetId() const override;
  std::string GetParentId() const override;
  std::string GetType() const override;
  std::string GetTitle() const override;
  std::string GetDescription() const override;
  GURL GetURL() const override;
  GURL GetFaviconURL() const override;
  base::TimeTicks GetLastActivityTime() const override;
  bool IsAttached() const override;
  scoped_refptr<content::DevToolsAgentHost> GetAgentHost() const override;
  bool Activate() const override;
  bool Close() const override;

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  GURL favicon_url_;
  base::TimeTicks last_activity_time_;
};

}  // namespace devtools_discovery

#endif  // COMPONENTS_DEVTOOLS_DISCOVERY_BASIC_TARGET_DESCRIPTOR_H_
