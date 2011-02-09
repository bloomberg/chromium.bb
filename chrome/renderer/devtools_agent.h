// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEVTOOLS_AGENT_H_
#define CHROME_RENDERER_DEVTOOLS_AGENT_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "chrome/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgentClient.h"

namespace WebKit {
class WebDevToolsAgent;
}

struct DevToolsMessageData;

typedef std::map<std::string, std::string> DevToolsRuntimeProperties;

// DevToolsAgent belongs to the inspectable RenderView and provides Glue's
// agents with the communication capabilities. All messages from/to Glue's
// agents infrastructure are flowing through this communication agent.
// There is a corresponding DevToolsClient object on the client side.
class DevToolsAgent : public RenderViewObserver,
                      public WebKit::WebDevToolsAgentClient {
 public:
  explicit DevToolsAgent(RenderView* render_view);
  virtual ~DevToolsAgent();

  // Returns agent instance for its host id.
  static DevToolsAgent* FromHostId(int host_id);

  WebKit::WebDevToolsAgent* GetWebAgent();

 private:
  friend class DevToolsAgentFilter;

  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // WebDevToolsAgentClient implementation
  virtual void sendMessageToInspectorFrontend(const WebKit::WebString& data);
  virtual void sendDebuggerOutput(const WebKit::WebString& data);

  virtual int hostIdentifier();
  virtual void runtimeFeatureStateChanged(const WebKit::WebString& feature,
                                          bool enabled);
  virtual void runtimePropertyChanged(const WebKit::WebString& name,
                                      const WebKit::WebString& value);
  virtual WebKit::WebCString debuggerScriptSource();
  virtual WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop*
      createClientMessageLoop();
  virtual bool exposeV8DebuggerProtocol();

  void OnAttach(const DevToolsRuntimeProperties& runtime_properties);
  void OnDetach();
  void OnFrontendLoaded();
  void OnDispatchOnInspectorBackend(const std::string& message);
  void OnInspectElement(int x, int y);
  void OnSetApuAgentEnabled(bool enabled);
  void OnNavigate();

  static std::map<int, DevToolsAgent*> agent_for_routing_id_;

  bool expose_v8_debugger_protocol_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgent);
};

#endif  // CHROME_RENDERER_DEVTOOLS_AGENT_H_
