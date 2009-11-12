// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBWORKER_PROXY_H_
#define CHROME_RENDERER_WEBWORKER_PROXY_H_

#include <vector>

#include "base/basictypes.h"
#include "chrome/renderer/webworker_base.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/WebKit/chromium/public/WebWorker.h"

class ChildThread;
class GURL;
class RenderView;
struct WorkerHostMsg_PostConsoleMessageToWorkerObject_Params;

// This class provides an implementation of WebWorker that the renderer provides
// to the glue.  This class converts function calls to IPC messages that are
// dispatched in the worker process by WebWorkerClientProxy.  It also receives
// IPC messages from WebWorkerClientProxy which it converts to function calls to
// WebWorkerClient.
class WebWorkerProxy : public WebKit::WebWorker, private WebWorkerBase {
 public:
  WebWorkerProxy(WebKit::WebWorkerClient* client,
                 ChildThread* child_thread,
                 int render_view_route_id);

  // WebWorker implementation.
  virtual void startWorkerContext(const WebKit::WebURL& script_url,
                                  const WebKit::WebString& user_agent,
                                  const WebKit::WebString& source_code);
  virtual void terminateWorkerContext();
  virtual void postMessageToWorkerContext(
      const WebKit::WebString& message,
      const WebKit::WebMessagePortChannelArray& channel_array);
  virtual void workerObjectDestroyed();
  virtual void clientDestroyed();

  // IPC::Channel::Listener implementation.
  void OnMessageReceived(const IPC::Message& message);

 private:
  virtual void Disconnect();

  void OnWorkerCreated();
  void OnWorkerContextDestroyed();
  void OnPostMessage(const string16& message,
                     const std::vector<int>& sent_message_port_ids,
                     const std::vector<int>& new_routing_ids);
  void OnPostConsoleMessageToWorkerObject(
      const WorkerHostMsg_PostConsoleMessageToWorkerObject_Params& params);


  // Used to communicate to the WebCore::Worker object in response to IPC
  // messages.
  WebKit::WebWorkerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(WebWorkerProxy);
};

#endif  // CHROME_RENDERER_WEBWORKER_PROXY_H_
