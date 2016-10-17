// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_OBSERVER_H_

#include "content/common/content_export.h"

namespace content {

class DevToolsAgentHost;

// Observer API notifies interested parties about changes in DevToolsAgentHosts.
class CONTENT_EXPORT DevToolsAgentHostObserver {
 public:
  virtual ~DevToolsAgentHostObserver() {}

  virtual void DevToolsAgentHostAttached(DevToolsAgentHost* agent_host) {}
  virtual void DevToolsAgentHostDetached(DevToolsAgentHost* agent_host) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_OBSERVER_H_
