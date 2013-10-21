// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_DISPATCHER_HOST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "content/common/content_export.h"
#include "content/common/websocket.h"
#include "content/public/browser/browser_message_filter.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace content {

class WebSocketHost;

// Creates a WebSocketHost object for each WebSocket channel, and dispatches
// WebSocketMsg_* messages sent from renderer to the appropriate WebSocketHost.
class CONTENT_EXPORT WebSocketDispatcherHost : public BrowserMessageFilter {
 public:
  typedef base::Callback<net::URLRequestContext*()> GetRequestContextCallback;

  // Given a routing_id, WebSocketHostFactory returns a new instance of
  // WebSocketHost or its subclass.
  typedef base::Callback<WebSocketHost*(int)> WebSocketHostFactory;

  explicit WebSocketDispatcherHost(
      const GetRequestContextCallback& get_context_callback);

  // For testing. Specify a factory method that creates mock version of
  // WebSocketHost.
  WebSocketDispatcherHost(
      const GetRequestContextCallback& get_context_callback,
      const WebSocketHostFactory& websocket_host_factory);

  // BrowserMessageFilter:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // The following methods are used by WebSocketHost::EventInterface to send
  // IPCs from the browser to the renderer or child process. Any of them may
  // delete the WebSocketHost on failure, leading to the WebSocketChannel and
  // EventInterface also being deleted.

  // Sends a WebSocketMsg_AddChannelResponse IPC, and then deletes and
  // unregisters the WebSocketHost if |fail| is true.
  void SendAddChannelResponse(int routing_id,
                              bool fail,
                              const std::string& selected_protocol,
                              const std::string& extensions);

  // Sends a WebSocketMsg_SendFrame IPC.
  void SendFrame(int routing_id,
                 bool fin,
                 WebSocketMessageType type,
                 const std::vector<char>& data);

  // Sends a WebSocketMsg_FlowControl IPC.
  void SendFlowControl(int routing_id, int64 quota);

  // Sends a WebSocketMsg_SendClosing IPC
  void SendClosing(int routing_id);

  // Sends a WebSocketMsg_DropChannel IPC and delete and unregister the channel.
  void DoDropChannel(int routing_id, uint16 code, const std::string& reason);

 private:
  typedef base::hash_map<int, WebSocketHost*> WebSocketHostTable;

  virtual ~WebSocketDispatcherHost();

  WebSocketHost* CreateWebSocketHost(int routing_id);

  // Looks up a WebSocketHost object by |routing_id|. Returns the object if one
  // is found, or NULL otherwise.
  WebSocketHost* GetHost(int routing_id) const;

  // Sends the passed in IPC::Message via the BrowserMessageFilter::Send()
  // method. If the Send() fails, logs it and deletes the corresponding
  // WebSocketHost object (the client is probably dead).
  void SendOrDrop(IPC::Message* message);

  // Deletes the WebSocketHost object associated with the given |routing_id| and
  // removes it from the |hosts_| table. Does nothing if the |routing_id| is
  // unknown, since SendAddChannelResponse() may call this method twice.
  void DeleteWebSocketHost(int routing_id);

  // Table of WebSocketHost objects, owned by this object, indexed by
  // routing_id.
  WebSocketHostTable hosts_;

  // A callback which returns the appropriate net::URLRequestContext for us to
  // use.
  GetRequestContextCallback get_context_callback_;

  WebSocketHostFactory websocket_host_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_WEBSOCKET_DISPATCHER_HOST_H_
