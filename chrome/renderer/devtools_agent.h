// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_DEVTOOLS_AGENT_H_
#define CHROME_RENDERER_DEVTOOLS_AGENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgentClient.h"

namespace IPC {
class Message;
}

namespace WebKit {
class WebDevToolsAgent;
}

class RenderView;
struct DevToolsMessageData;

// DevToolsAgent belongs to the inspectable RenderView and provides Glue's
// agents with the communication capabilities. All messages from/to Glue's
// agents infrastructure are flowing through this comminucation agent.
// There is a corresponding DevToolsClient object on the client side.
class DevToolsAgent : public WebKit::WebDevToolsAgentClient {
 public:
  DevToolsAgent(int routing_id, RenderView* view);
  virtual ~DevToolsAgent();

  void OnNavigate();

  // IPC message interceptor. Called on the Render thread.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // WebDevToolsAgentClient implementation
  virtual void sendMessageToFrontend(
      const WebKit::WebDevToolsMessageData& data);

  virtual int hostIdentifier();
  virtual void forceRepaint();
  virtual void runtimeFeatureStateChanged(const WebKit::WebString& feature,
                                          bool enabled);
  virtual WebKit::WebCString injectedScriptSource();
  virtual WebKit::WebCString injectedScriptDispatcherSource();

  // Returns agent instance for its host id.
  static DevToolsAgent* FromHostId(int host_id);

  RenderView* render_view() { return render_view_; }

  WebKit::WebDevToolsAgent* GetWebAgent();

 private:
  friend class DevToolsAgentFilter;

  void OnAttach(const std::vector<std::string>& runtime_features);
  void OnDetach();
  void OnRpcMessage(const DevToolsMessageData& data);
  void OnInspectElement(int x, int y);
  void OnSetApuAgentEnabled(bool enabled);

  static std::map<int, DevToolsAgent*> agent_for_routing_id_;

  int routing_id_; //  View routing id that we can access from IO thread.
  RenderView* render_view_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgent);
};

#endif  // CHROME_RENDERER_DEVTOOLS_AGENT_H_
