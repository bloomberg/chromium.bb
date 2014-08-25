// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "url/gurl.h"

namespace content {

class DevToolsExternalAgentProxyDelegate;
class WebContents;

// Describes interface for managing devtools agents from browser process.
class CONTENT_EXPORT DevToolsAgentHost
    : public base::RefCounted<DevToolsAgentHost> {
 public:
  enum Type {
    // Agent host associated with WebContents.
    TYPE_WEB_CONTENTS,

    // Agent host associated with shared worker.
    TYPE_SHARED_WORKER,

    // Agent host associated with service worker.
    TYPE_SERVICE_WORKER,

    // Agent host associated with DevToolsExternalAgentProxyDelegate.
    TYPE_EXTERNAL,
  };

  // Returns DevToolsAgentHost with a given |id| or NULL of it does not exist.
  static scoped_refptr<DevToolsAgentHost> GetForId(const std::string& id);

  // Returns DevToolsAgentHost that can be used for inspecting |web_contents|.
  // New DevToolsAgentHost will be created if it does not exist.
  static scoped_refptr<DevToolsAgentHost> GetOrCreateFor(
      WebContents* web_contents);

  // Returns true iff an instance of DevToolsAgentHost for the |web_contents|
  // does exist.
  static bool HasFor(WebContents* web_contents);

  // Returns DevToolsAgentHost that can be used for inspecting shared worker
  // with given worker process host id and routing id.
  static scoped_refptr<DevToolsAgentHost> GetForWorker(int worker_process_id,
                                                       int worker_route_id);

  // Creates DevToolsAgentHost that communicates to the target by means of
  // provided |delegate|. |delegate| ownership is passed to the created agent
  // host.
  static scoped_refptr<DevToolsAgentHost> Create(
      DevToolsExternalAgentProxyDelegate* delegate);

  static bool IsDebuggerAttached(WebContents* web_contents);

  typedef std::vector<scoped_refptr<DevToolsAgentHost> > List;

  // Returns all possible DevToolsAgentHosts.
  static List GetOrCreateAll();

  // Client attaches to this agent host to start debugging it.
  virtual void AttachClient(DevToolsAgentHostClient* client) = 0;

  // Already attached client detaches from this agent host to stop debugging it.
  virtual void DetachClient() = 0;

  // Returns true if there is a client attached.
  virtual bool IsAttached() = 0;

  // Sends a message to the agent.
  virtual void DispatchProtocolMessage(const std::string& message) = 0;

  // Starts inspecting element at position (|x|, |y|) in the specified page.
  virtual void InspectElement(int x, int y) = 0;

  // Returns the unique id of the agent.
  virtual std::string GetId() = 0;

  // Returns web contents instance for this host if any.
  virtual WebContents* GetWebContents() = 0;

  // Temporarily detaches render view host from this host. Must be followed by
  // a call to ConnectWebContents (may leak the host instance otherwise).
  virtual void DisconnectWebContents() = 0;

  // Attaches render view host to this host.
  virtual void ConnectWebContents(WebContents* web_contents) = 0;

  // Returns true if DevToolsAgentHost is for worker.
  virtual bool IsWorker() const = 0;

  // Returns agent host type.
  virtual Type GetType() = 0;

  // Returns agent host title.
  virtual std::string GetTitle() = 0;

  // Returns url associated with agent host.
  virtual GURL GetURL() = 0;

  // Activates agent host. Returns false if the operation failed.
  virtual bool Activate() = 0;

  // Closes agent host. Returns false if the operation failed.
  virtual bool Close() = 0;

  // Terminates all debugging sessions and detaches all clients.
  static void DetachAllClients();

  typedef base::Callback<void(DevToolsAgentHost*, bool attached)>
      AgentStateCallback;

  static void AddAgentStateCallback(const AgentStateCallback& callback);
  static void RemoveAgentStateCallback(const AgentStateCallback& callback);

 protected:
  friend class base::RefCounted<DevToolsAgentHost>;
  virtual ~DevToolsAgentHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_AGENT_HOST_H_
