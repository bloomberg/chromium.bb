// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_REGISTRY_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_REGISTRY_H_

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

  // Detaches given |rvh| from the agent host temporarily and returns a cookie
  // that allows to reattach another rvh to that agen thost later. Returns -1 if
  // there is no agent host associated with the |rvh|.
  static int DisconnectRenderViewHost(RenderViewHost* rvh);

  // Reattaches agent host detached with DisconnectRenderViewHost method above
  // to |rvh|.
  static void ConnectRenderViewHost(int agent_host_cookie,
                                    RenderViewHost* rvh);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_REGISTRY_H_
