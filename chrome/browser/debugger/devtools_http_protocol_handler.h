// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_

#include <set>
#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/server/http_listen_socket.h"
#include "net/url_request/url_request.h"

class DevToolsClientHost;
class DevToolsHttpServer;

class DevToolsHttpProtocolHandler
    : public HttpListenSocket::Delegate,
      public URLRequest::Delegate,
      public base::RefCountedThreadSafe<DevToolsHttpProtocolHandler> {
 public:
  explicit DevToolsHttpProtocolHandler(int port);

  // This method should be called after the object construction.
  void Start();

  // This method should be called before the object destruction.
  void Stop();

  // HttpListenSocket::Delegate implementation.
  virtual void OnHttpRequest(HttpListenSocket* socket,
                             HttpServerRequestInfo* info);
  virtual void OnWebSocketRequest(HttpListenSocket* socket,
                                  HttpServerRequestInfo* info);
  virtual void OnWebSocketMessage(HttpListenSocket* socket,
                                  const std::string& data);
  virtual void OnClose(HttpListenSocket* socket);

  // URLRequest::Delegate implementation.
  virtual void OnResponseStarted(URLRequest* request);
  virtual void OnReadCompleted(URLRequest* request, int bytes_read);

 private:
  friend class base::RefCountedThreadSafe<DevToolsHttpProtocolHandler>;
  virtual ~DevToolsHttpProtocolHandler();

  void Init();
  void Teardown();
  void Bind(URLRequest* request, HttpListenSocket* socket);
  void RequestCompleted(URLRequest* request);
  void OnWebSocketMessageUI(HttpListenSocket* socket, const std::string& data);

  int port_;
  scoped_refptr<HttpListenSocket> server_;
  typedef std::map<URLRequest*, HttpListenSocket*>
      RequestToSocketMap;
  RequestToSocketMap request_to_socket_;
  typedef std::map<HttpListenSocket*, std::set<URLRequest*> >
      SocketToRequestsMap;
  SocketToRequestsMap socket_to_requests_;
  typedef std::map<URLRequest*, scoped_refptr<net::IOBuffer> >
      BuffersMap;
  BuffersMap request_to_buffer_;
  scoped_ptr<DevToolsClientHost> client_host_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpProtocolHandler);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
