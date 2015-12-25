// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/console_message_level.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebDevToolsAgentClient.h"

namespace blink {
class WebDevToolsAgent;
}

namespace content {

class DevToolsCPUThrottler;
class RenderFrameImpl;

// DevToolsAgent belongs to the inspectable RenderFrameImpl and communicates
// with WebDevToolsAgent. There is a corresponding DevToolsAgentHost
// on the browser side.
class CONTENT_EXPORT DevToolsAgent
    : public RenderFrameObserver,
      NON_EXPORTED_BASE(public blink::WebDevToolsAgentClient) {
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

  blink::WebDevToolsAgent* GetWebAgent();

  bool IsAttached();

  void AddMessageToConsole(ConsoleMessageLevel level,
                           const std::string& message);

 private:
  friend class DevToolsAgentTest;

  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void WidgetWillClose() override;

  // WebDevToolsAgentClient implementation.
  void sendProtocolMessage(int session_id,
                           int call_id,
                           const blink::WebString& response,
                           const blink::WebString& state) override;
  blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
      createClientMessageLoop() override;
  void willEnterDebugLoop() override;
  void didExitDebugLoop() override;

  void enableTracing(const blink::WebString& category_filter) override;
  void disableTracing() override;

  void setCPUThrottlingRate(double rate) override;

  void OnAttach(const std::string& host_id, int session_id);
  void OnReattach(const std::string& host_id,
                  int session_id,
                  const std::string& agent_state);
  void OnDetach();
  void OnDispatchOnInspectorBackend(int session_id, const std::string& message);
  void OnInspectElement(int x, int y);
  void ContinueProgram();
  void OnSetupDevToolsClient(const std::string& compatibility_script);

  bool is_attached_;
  bool is_devtools_client_;
  bool paused_in_mouse_move_;
  bool paused_;
  RenderFrameImpl* frame_;
  scoped_ptr<DevToolsCPUThrottler> cpu_throttler_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgent);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_
