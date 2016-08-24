// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "components/devtools_discovery/basic_target_descriptor.h"

class Profile;

namespace content {
class DevToolsAgentHost;
class WebContents;
}

class DevToolsTargetImpl : public devtools_discovery::BasicTargetDescriptor {
 public:
  explicit DevToolsTargetImpl(
      scoped_refptr<content::DevToolsAgentHost> agent_host);
  ~DevToolsTargetImpl() override;

  // Caller takes ownership of returned objects.
  static std::vector<DevToolsTargetImpl*> EnumerateAll();
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_
