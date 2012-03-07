// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_REGISTRY_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_REGISTRY_H_
#pragma once

#include "content/common/content_export.h"

namespace content {

class DevToolsAgentHost;
class RenderViewHost;
class WebContents;

class CONTENT_EXPORT DevToolsAgentHostRegistry {
 public:
  // Returns DevToolsAgentHost that can be used for inspecting |rvh|.
  // New DevToolsAgentHost will be created if it does not exist.
  static DevToolsAgentHost* GetDevToolsAgentHost(RenderViewHost* rvh);

  // Returns render view host instance for given |agent_host|.
  static RenderViewHost* GetRenderViewHost(
      DevToolsAgentHost* agent_host);

  // Returns true iff an instance of DevToolsAgentHost for the |rvh|
  // does exist.
  static bool HasDevToolsAgentHost(RenderViewHost* rvh);

  // Returns DevToolsAgentHost that can be used for inspecting shared worker
  // with given worker process host id and routing id.
  static DevToolsAgentHost* GetDevToolsAgentHostForWorker(
      int worker_process_id,
      int worker_route_id);

  static bool IsDebuggerAttached(WebContents* web_contents);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_REGISTRY_H_
