// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/server/http_server.h"
#include "net/url_request/url_request.h"

class DevToolsClientHost;
class DevToolsHttpServer;
class TabContents;
class TabContentsWrapper;

class DevToolsHttpProtocolHandler
    : public HttpServer::Delegate,
      public net::URLRequest::Delegate,
      public base::RefCountedThreadSafe<DevToolsHttpProtocolHandler> {
 public:
  typedef std::vector<TabContentsWrapper*> InspectableTabs;
  class TabContentsProvider {
   public:
    TabContentsProvider() {}
    virtual ~TabContentsProvider() {}
    virtual InspectableTabs GetInspectableTabs() = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(TabContentsProvider);
  };

  // Takes ownership over |provider|.
  static scoped_refptr<DevToolsHttpProtocolHandler> Start(
      int port,
      const std::string& frontend_url,
      TabContentsProvider* provider);

  // Called from the main thread in order to stop protocol handler.
  // Will schedule tear down task on IO thread.
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<DevToolsHttpProtocolHandler>;

  DevToolsHttpProtocolHandler(int port,
                              const std::string& frontend_url,
                              TabContentsProvider* provider);
  virtual ~DevToolsHttpProtocolHandler();
  void Start();

  // HttpServer::Delegate implementation.
  virtual void OnHttpRequest(int connection_id,
                             const HttpServerRequestInfo& info);
  virtual void OnWebSocketRequest(int connection_id,
                                  const HttpServerRequestInfo& info);
  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data);
  virtual void OnClose(int connection_id);

  virtual void OnHttpRequestUI(int connection_id,
                               const HttpServerRequestInfo& info);
  virtual void OnWebSocketRequestUI(int connection_id,
                                    const HttpServerRequestInfo& info);
  virtual void OnWebSocketMessageUI(int connection_id,
                                    const std::string& data);
  virtual void OnCloseUI(int connection_id);

  // net::URLRequest::Delegate implementation.
  virtual void OnResponseStarted(net::URLRequest* request);
  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read);

  void Init();
  void Teardown();
  void Bind(net::URLRequest* request, int connection_id);
  void RequestCompleted(net::URLRequest* request);

  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type = "text/html");
  void Send404(int connection_id);
  void Send500(int connection_id,
               const std::string& message);
  void AcceptWebSocket(int connection_id,
                       const HttpServerRequestInfo& request);

  TabContents* GetTabContents(int session_id);

  int port_;
  std::string overriden_frontend_url_;
  scoped_refptr<HttpServer> server_;
  typedef std::map<net::URLRequest*, int>
      RequestToSocketMap;
  RequestToSocketMap request_to_connection_io_;
  typedef std::map<int, std::set<net::URLRequest*> >
      ConnectionToRequestsMap;
  ConnectionToRequestsMap connection_to_requests_io_;
  typedef std::map<net::URLRequest*, scoped_refptr<net::IOBuffer> >
      BuffersMap;
  BuffersMap request_to_buffer_io_;
  typedef std::map<int, DevToolsClientHost*>
      ConnectionToClientHostMap;
  ConnectionToClientHostMap connection_to_client_host_ui_;
  scoped_ptr<TabContentsProvider> tab_contents_provider_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpProtocolHandler);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
