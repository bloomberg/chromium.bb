// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
#pragma once

#include <set>
#include <string>

#include "base/ref_counted.h"
#include "net/server/http_listen_socket.h"
#include "net/url_request/url_request.h"

class DevToolsClientHost;
class DevToolsHttpServer;
class TabContents;

class DevToolsHttpProtocolHandler
    : public HttpListenSocket::Delegate,
      public net::URLRequest::Delegate,
      public base::RefCountedThreadSafe<DevToolsHttpProtocolHandler> {
 public:
  static scoped_refptr<DevToolsHttpProtocolHandler> Start(
      int port,
      const std::string& frontend_url);

  // Called from the main thread in order to stop protocol handler.
  // Will schedule tear down task on IO thread.
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<DevToolsHttpProtocolHandler>;

  DevToolsHttpProtocolHandler(int port, const std::string& frontend_url);
  virtual ~DevToolsHttpProtocolHandler();
  void Start();

  // HttpListenSocket::Delegate implementation.
  virtual void OnHttpRequest(HttpListenSocket* socket,
                             const HttpServerRequestInfo& info);
  virtual void OnWebSocketRequest(HttpListenSocket* socket,
                                  const HttpServerRequestInfo& info);
  virtual void OnWebSocketMessage(HttpListenSocket* socket,
                                  const std::string& data);
  virtual void OnClose(HttpListenSocket* socket);

  virtual void OnHttpRequestUI(HttpListenSocket* socket,
                               const HttpServerRequestInfo& info);
  virtual void OnWebSocketRequestUI(HttpListenSocket* socket,
                                    const HttpServerRequestInfo& info);
  virtual void OnWebSocketMessageUI(HttpListenSocket* socket,
                                    const std::string& data);
  virtual void OnCloseUI(HttpListenSocket* socket);

  // net::URLRequest::Delegate implementation.
  virtual void OnResponseStarted(net::URLRequest* request);
  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read);

  void Init();
  void Teardown();
  void Bind(net::URLRequest* request, HttpListenSocket* socket);
  void RequestCompleted(net::URLRequest* request);

  void Send200(HttpListenSocket* socket,
               const std::string& data,
               const std::string& mime_type = "text/html");
  void Send404(HttpListenSocket* socket);
  void Send500(HttpListenSocket* socket,
               const std::string& message);
  void AcceptWebSocket(HttpListenSocket* socket,
                       const HttpServerRequestInfo& request);
  void ReleaseSocket(HttpListenSocket* socket);

  TabContents* GetTabContents(int session_id);

  int port_;
  std::string overriden_frontend_url_;
  scoped_refptr<HttpListenSocket> server_;
  typedef std::map<net::URLRequest*, HttpListenSocket*>
      RequestToSocketMap;
  RequestToSocketMap request_to_socket_io_;
  typedef std::map<HttpListenSocket*, std::set<net::URLRequest*> >
      SocketToRequestsMap;
  SocketToRequestsMap socket_to_requests_io_;
  typedef std::map<net::URLRequest*, scoped_refptr<net::IOBuffer> >
      BuffersMap;
  BuffersMap request_to_buffer_io_;
  typedef std::map<HttpListenSocket*, DevToolsClientHost*>
      SocketToClientHostMap;
  SocketToClientHostMap socket_to_client_host_ui_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpProtocolHandler);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
