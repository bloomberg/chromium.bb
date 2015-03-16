// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_

#include <string>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/time/time.h"
#include "content/public/common/console_message_level.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebDevToolsAgentClient.h"

namespace blink {
class WebDevToolsAgent;
}

namespace content {

class RenderViewImpl;

// DevToolsAgent belongs to the inspectable RenderView and provides Glue's
// agents with the communication capabilities. All messages from/to Glue's
// agents infrastructure are flowing through this communication agent.
// There is a corresponding DevToolsClient object on the client side.
class DevToolsAgent : public RenderFrameObserver,
                      public blink::WebDevToolsAgentClient {
 public:
  explicit DevToolsAgent(RenderFrame* main_render_frame);
  ~DevToolsAgent() override;

  // Returns agent instance for its routing id.
  static DevToolsAgent* FromRoutingId(int routing_id);

  static void SendChunkedProtocolMessage(
      IPC::Sender* sender,
      int routing_id,
      int call_id,
      const std::string& message,
      const std::string& post_state);

  blink::WebDevToolsAgent* GetWebAgent();

  bool IsAttached();

 private:
  // RenderView::Observer implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // WebDevToolsAgentClient implementation
  void sendProtocolMessage(int call_id,
                           const blink::WebString& response,
                           const blink::WebString& state) override;
  long processId() override;
  int debuggerId() override;
  blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
      createClientMessageLoop() override;
  void willEnterDebugLoop() override;
  void didExitDebugLoop() override;

  typedef void (*TraceEventCallback)(
      char phase, const unsigned char*, const char* name, unsigned long long id,
      int numArgs, const char* const* argNames, const unsigned char* argTypes,
      const unsigned long long* argValues,
      unsigned char flags, double timestamp);
  void resetTraceEventCallback() override;
  void setTraceEventCallback(const blink::WebString& category_filter,
                             TraceEventCallback cb) override;
  void enableTracing(const blink::WebString& category_filter) override;
  void disableTracing() override;

  void OnAttach(const std::string& host_id);
  void OnReattach(const std::string& host_id,
                  const std::string& agent_state);
  void OnDetach();
  void OnDispatchOnInspectorBackend(const std::string& message);
  void OnInspectElement(const std::string& host_id, int x, int y);
  void OnAddMessageToConsole(ConsoleMessageLevel level,
                             const std::string& message);
  void ContinueProgram();
  void OnSetupDevToolsClient();

  RenderViewImpl* GetRenderViewImpl();

  static void TraceEventCallbackWrapper(
      base::TimeTicks timestamp,
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      unsigned long long id,
      int num_args,
      const char* const arg_names[],
      const unsigned char arg_types[],
      const unsigned long long arg_values[],
      unsigned char flags);

  bool is_attached_;
  bool is_devtools_client_;
  bool paused_in_mouse_move_;
  RenderFrame* main_render_frame_;

  static base::subtle::AtomicWord /* TraceEventCallback */ event_callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgent);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_
