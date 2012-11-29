// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_WEBSOCKET_H_
#define CHROME_TEST_CHROMEDRIVER_NET_WEBSOCKET_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/socket_stream/socket_stream.h"
#include "net/websockets/websocket_frame_parser.h"

namespace net {
class URLRequestContextGetter;
class WebSocketJob;
}  // namespace net

class WebSocketListener;

// A text-only, non-thread safe WebSocket. Must be created and used on a single
// thread. Intended particularly for use with net::HttpServer.
class WebSocket : public net::SocketStream::Delegate {
 public:
  WebSocket(net::URLRequestContextGetter* context_getter,
            const GURL& url,
            WebSocketListener* listener);
  virtual ~WebSocket();

  // Initializes the WebSocket connection. Invokes the given callback with
  // a net::Error. May only be called once.
  void Connect(const net::CompletionCallback& callback);

  // Sends the given message and returns true on success.
  bool Send(const std::string& message);

  // Overridden from net::SocketStream::Delegate:
  virtual void OnConnected(net::SocketStream* socket,
                           int max_pending_send_allowed) OVERRIDE;
  virtual void OnSentData(net::SocketStream* socket,
                          int amount_sent) OVERRIDE;
  virtual void OnReceivedData(net::SocketStream* socket,
                              const char* data,
                              int len) OVERRIDE;
  virtual void OnClose(net::SocketStream* socket) OVERRIDE;

 private:
  void OnConnectFinished(net::Error error);

  base::ThreadChecker thread_checker_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  GURL url_;
  scoped_refptr<net::WebSocketJob> web_socket_;
  WebSocketListener* listener_;
  net::CompletionCallback connect_callback_;
  std::string sec_key_;
  net::WebSocketFrameParser parser_;
  std::string next_message_;
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

// Listens for WebSocket messages and disconnects on the same thread as the
// WebSocket.
class WebSocketListener {
 public:
  virtual ~WebSocketListener() {}

  // Called when a WebSocket message is received.
  virtual void OnMessageReceived(const std::string& message) = 0;

  // Called when the WebSocket connection closes. Will be called at most once.
  // Will not be called if the connection was never established or if the close
  // was initiated by the client.
  virtual void OnClose() = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_WEBSOCKET_H_
