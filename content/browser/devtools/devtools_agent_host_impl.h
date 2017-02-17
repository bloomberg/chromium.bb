// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/common/content_export.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

class BrowserContext;
class DevToolsSession;

namespace protocol {
class DevToolsDomainHandler;
}

// Describes interface for managing devtools agents from the browser process.
class CONTENT_EXPORT DevToolsAgentHostImpl : public DevToolsAgentHost {
 public:
  // DevToolsAgentHost implementation.
  bool AttachClient(DevToolsAgentHostClient* client) override;
  void ForceAttachClient(DevToolsAgentHostClient* client) override;
  bool DetachClient(DevToolsAgentHostClient* client) override;
  bool DispatchProtocolMessage(DevToolsAgentHostClient* client,
                               const std::string& message) override;
  bool IsAttached() override;
  void InspectElement(DevToolsAgentHostClient* client, int x, int y) override;
  std::string GetId() override;
  std::string GetParentId() override;
  std::string GetDescription() override;
  GURL GetFaviconURL() override;
  std::string GetFrontendURL() override;
  base::TimeTicks GetLastActivityTime() override;
  BrowserContext* GetBrowserContext() override;
  WebContents* GetWebContents() override;
  void DisconnectWebContents() override;
  void ConnectWebContents(WebContents* wc) override;

  bool Inspect();
  void SendMessageToClient(int session_id, const std::string& message);

 protected:
  DevToolsAgentHostImpl(const std::string& id);
  ~DevToolsAgentHostImpl() override;

  static bool ShouldForceCreation();

  virtual void AttachSession(DevToolsSession* session) = 0;
  virtual void DetachSession(int session_id) = 0;
  virtual bool DispatchProtocolMessage(
      DevToolsSession* session,
      const std::string& message) = 0;
  virtual void InspectElement(DevToolsSession* session, int x, int y);

  void NotifyCreated();
  void ForceDetach(bool replaced);
  DevToolsIOContext* GetIOContext() { return &io_context_; }

  // TODO(dgozman): remove this accessor.
  DevToolsSession* session() { return session_.get(); }

 private:
  friend class DevToolsAgentHost; // for static methods
  friend class protocol::DevToolsDomainHandler;
  bool InnerAttachClient(DevToolsAgentHostClient* client, bool force);
  void InnerDetachClient();
  void NotifyAttached();
  void NotifyDetached();
  void NotifyDestroyed();

  const std::string id_;
  int last_session_id_;
  std::unique_ptr<DevToolsSession> session_;
  DevToolsIOContext io_context_;
  static int s_attached_count_;
  static int s_force_creation_count_;
};

class DevToolsMessageChunkProcessor {
 public:
  using SendMessageCallback = base::Callback<void(int, const std::string&)>;
  explicit DevToolsMessageChunkProcessor(const SendMessageCallback& callback);
  ~DevToolsMessageChunkProcessor();

  std::string state_cookie() const { return state_cookie_; }
  void set_state_cookie(const std::string& cookie) { state_cookie_ = cookie; }
  int last_call_id() const { return last_call_id_; }
  bool ProcessChunkedMessageFromAgent(const DevToolsMessageChunk& chunk);

 private:
  SendMessageCallback callback_;
  std::string message_buffer_;
  uint32_t message_buffer_size_;
  std::string state_cookie_;
  int last_call_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_AGENT_HOST_IMPL_H_
