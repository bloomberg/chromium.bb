// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_

#include <string>

#include "base/basictypes.h"
#include "content/public/common/console_message_level.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgentClient.h"

namespace WebKit {
class WebDevToolsAgent;
}

namespace content {
class RenderViewImpl;

// DevToolsAgent belongs to the inspectable RenderView and provides Glue's
// agents with the communication capabilities. All messages from/to Glue's
// agents infrastructure are flowing through this communication agent.
// There is a corresponding DevToolsClient object on the client side.
class DevToolsAgent : public RenderViewObserver,
                      public WebKit::WebDevToolsAgentClient {
 public:
  explicit DevToolsAgent(RenderViewImpl* render_view);
  virtual ~DevToolsAgent();

  // Returns agent instance for its host id.
  static DevToolsAgent* FromHostId(int host_id);

  WebKit::WebDevToolsAgent* GetWebAgent();

  bool IsAttached();

 private:
  friend class DevToolsAgentFilter;

  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebDevToolsAgentClient implementation
  virtual void sendMessageToInspectorFrontend(const WebKit::WebString& data);

  virtual int hostIdentifier();
  virtual void saveAgentRuntimeState(const WebKit::WebString& state);
  virtual WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop*
      createClientMessageLoop();
  virtual void clearBrowserCache();
  virtual void clearBrowserCookies();
  virtual void visitAllocatedObjects(AllocatedObjectVisitor* visitor);
  virtual void setTraceEventCallback(TraceEventCallback cb);

  void OnAttach();
  void OnReattach(const std::string& agent_state);
  void OnDetach();
  void OnDispatchOnInspectorBackend(const std::string& message);
  void OnInspectElement(int x, int y);
  void OnAddMessageToConsole(ConsoleMessageLevel level,
                             const std::string& message);
  void ContinueProgram();
  void OnSetupDevToolsClient();

  bool is_attached_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgent);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_AGENT_H_
