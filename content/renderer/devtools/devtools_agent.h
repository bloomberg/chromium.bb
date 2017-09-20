// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebDevToolsAgentClient.h"

namespace blink {
class WebDevToolsAgent;
}

class GURL;

namespace content {

class RenderFrameImpl;
struct Manifest;
struct ManifestDebugInfo;

// DevToolsAgent belongs to the inspectable RenderFrameImpl and communicates
// with WebDevToolsAgent. There is a corresponding DevToolsAgentHost
// on the browser side.
class CONTENT_EXPORT DevToolsAgent : public RenderFrameObserver,
                                     public blink::WebDevToolsAgentClient {
 public:
  explicit DevToolsAgent(RenderFrameImpl* frame);
  ~DevToolsAgent() override;

  // Returns agent instance for its routing id.
  static DevToolsAgent* FromRoutingId(int routing_id);

  static void SendChunkedProtocolMessage(IPC::Sender* sender,
                                         int routing_id,
                                         int session_id,
                                         int call_id,
                                         const std::string& message,
                                         const std::string& post_state);
  static blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
      createMessageLoopWrapper();

  blink::WebDevToolsAgent* GetWebAgent();

  bool IsAttached();
  void DetachAllSessions();

 private:
  friend class DevToolsAgentTest;

  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void WidgetWillClose() override;
  void OnDestruct() override;

  // WebDevToolsAgentClient implementation.
  void SendProtocolMessage(int session_id,
                           int call_id,
                           const blink::WebString& response,
                           const blink::WebString& state) override;
  blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
  CreateClientMessageLoop() override;
  void WillEnterDebugLoop() override;
  void DidExitDebugLoop() override;

  bool RequestDevToolsForFrame(blink::WebLocalFrame* frame) override;

  void EnableTracing(const blink::WebString& category_filter) override;
  void DisableTracing() override;

  void SetCPUThrottlingRate(double rate) override;

  void OnAttach(const std::string& host_id, int session_id);
  void OnReattach(const std::string& host_id,
                  int session_id,
                  const std::string& agent_state);
  void OnDetach(int session_id);
  void OnDispatchOnInspectorBackend(int session_id,
                                    int call_id,
                                    const std::string& method,
                                    const std::string& message);
  void OnInspectElement(int session_id, int x, int y);
  void OnRequestNewWindowACK(bool success);
  void ContinueProgram();
  void OnSetupDevToolsClient(const std::string& api_script);

  void GotManifest(int session_id,
                   int command_id,
                   const GURL& manifest_url,
                   const Manifest& manifest,
                   const ManifestDebugInfo& debug_info);

  std::set<int> session_ids_;
  bool is_devtools_client_;
  bool paused_;
  RenderFrameImpl* frame_;
  base::Callback<void(int, int, const std::string&, const std::string&)>
      send_protocol_message_callback_for_test_;
  base::WeakPtrFactory<DevToolsAgent> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgent);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_
