// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

class RenderViewHost;
class WebContents;

// Describes interface for managing devtools agents from browser process.
class CONTENT_EXPORT DevToolsAgentHost
    : public base::RefCounted<DevToolsAgentHost> {
 public:
  // Returns DevToolsAgentHost that can be used for inspecting |rvh|.
  // New DevToolsAgentHost will be created if it does not exist.
  static scoped_refptr<DevToolsAgentHost> GetFor(RenderViewHost* rvh);

  // Returns true iff an instance of DevToolsAgentHost for the |rvh|
  // does exist.
  static bool HasFor(RenderViewHost* rvh);

  // Returns DevToolsAgentHost that can be used for inspecting shared worker
  // with given worker process host id and routing id.
  static scoped_refptr<DevToolsAgentHost> GetForWorker(int worker_process_id,
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

  // Returns render view host instance for this host if any.
  virtual RenderViewHost* GetRenderViewHost() = 0;

 protected:
  friend class base::RefCounted<DevToolsAgentHost>;
  virtual ~DevToolsAgentHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_
