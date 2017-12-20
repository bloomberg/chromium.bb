// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/common/content_export.h"
#include "content/common/devtools.mojom.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

class BrowserContext;
class DevToolsSession;

// Describes interface for managing devtools agents from the browser process.
class CONTENT_EXPORT DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  // Asks any interested agents to handle the given certificate error. Returns
  // |true| if the error was handled, |false| otherwise.
  using CertErrorCallback =
      base::RepeatingCallback<void(content::CertificateRequestResultType)>;
  static bool HandleCertificateError(WebContents* web_contents,
                                     int cert_error,
                                     const GURL& request_url,
                                     CertErrorCallback callback);

  // DevToolsAgentHost implementation.
  void AttachClient(DevToolsAgentHostClient* client) override;
  void ForceAttachClient(DevToolsAgentHostClient* client) override;
  bool DetachClient(DevToolsAgentHostClient* client) override;
  bool DispatchProtocolMessage(DevToolsAgentHostClient* client,
                               const std::string& message) override;
  bool IsAttached() override;
  void InspectElement(DevToolsAgentHostClient* client, int x, int y) override;
  std::string GetId() override;
  std::string GetParentId() override;
  std::string GetOpenerId() override;
  std::string GetDescription() override;
  GURL GetFaviconURL() override;
  std::string GetFrontendURL() override;
  base::TimeTicks GetLastActivityTime() override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* wc) override;

  bool Inspect();

 protected:
  DevToolsAgentHostImpl(const std::string& id);
  ~DevToolsAgentHostImpl() override;

  static bool ShouldForceCreation();

  virtual void AttachSession(DevToolsSession* session) = 0;
  virtual void DetachSession(int session_id) = 0;
  virtual bool DispatchProtocolMessage(
      DevToolsSession* session,
      const std::string& message) = 0;
  bool SendProtocolMessageToClient(int session_id,
                                   const std::string& message) override;
  virtual void InspectElement(DevToolsSession* session, int x, int y);

  void NotifyCreated();
  void NotifyNavigated();
  void ForceDetachAllClients();
  void ForceDetachSession(DevToolsSession* session);
  DevToolsIOContext* GetIOContext() { return &io_context_; }

  base::flat_set<DevToolsSession*>& sessions() { return sessions_; }
  DevToolsSession* SessionById(int session_id);

 private:
  friend class DevToolsAgentHost; // for static methods
  friend class DevToolsSession;
  void InnerAttachClient(DevToolsAgentHostClient* client);
  void InnerDetachClient(DevToolsAgentHostClient* client);
  void NotifyAttached();
  void NotifyDetached();
  void NotifyDestroyed();
  DevToolsSession* SessionByClient(DevToolsAgentHostClient* client);

  const std::string id_;
  base::flat_set<DevToolsSession*> sessions_;
  base::flat_map<int, DevToolsSession*> session_by_id_;
  base::flat_map<DevToolsAgentHostClient*, std::unique_ptr<DevToolsSession>>
      session_by_client_;
  DevToolsIOContext io_context_;
  static int s_force_creation_count_;
  static int s_last_session_id_;
};

class DevToolsMessageChunkProcessor {
 public:
  using SendMessageIPCCallback = base::Callback<void(int, const std::string&)>;
  using SendMessageCallback = base::Callback<void(const std::string&)>;
  DevToolsMessageChunkProcessor(const SendMessageIPCCallback& ipc_callback,
                                const SendMessageCallback& callback);
  ~DevToolsMessageChunkProcessor();

  const std::string& state_cookie() const { return state_cookie_; }
  void set_state_cookie(const std::string& cookie) { state_cookie_ = cookie; }
  int last_call_id() const { return last_call_id_; }
  bool ProcessChunkedMessageFromAgent(const DevToolsMessageChunk& chunk);
  bool ProcessChunkedMessageFromAgent(mojom::DevToolsMessageChunkPtr chunk);
  void Reset();

 private:
  SendMessageIPCCallback ipc_callback_;
  SendMessageCallback callback_;
  std::string message_buffer_;
  uint32_t message_buffer_size_;
  std::string state_cookie_;
  int last_call_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
