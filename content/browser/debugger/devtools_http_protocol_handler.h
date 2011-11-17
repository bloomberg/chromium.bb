// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
#define CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "net/server/http_server.h"
#include "net/url_request/url_request.h"

class DevToolsClientHost;
class TabContents;

namespace net {
class URLRequestContext;
}

class DevToolsHttpProtocolHandler
    : public net::HttpServer::Delegate,
      public net::URLRequest::Delegate,
      public base::RefCountedThreadSafe<DevToolsHttpProtocolHandler> {
 public:
  typedef std::vector<TabContents*> InspectableTabs;
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Should return the list of inspectable tabs. Called on the UI thread.
    virtual InspectableTabs GetInspectableTabs() = 0;

    // Should return discovery page HTML that should list available tabs
    // and provide attach links. Called on the IO thread.
    virtual std::string GetDiscoveryPageHTML() = 0;

    // Should return URL request context for issuing requests against devtools
    // webui or NULL if no context is available. Called on the IO thread.
    virtual net::URLRequestContext* GetURLRequestContext() = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Takes ownership over |delegate|.
  CONTENT_EXPORT static scoped_refptr<DevToolsHttpProtocolHandler> Start(
      const std::string& ip,
      int port,
      const std::string& frontend_url,
      Delegate* delegate);

  // Called from the main thread in order to stop protocol handler.
  // Will schedule tear down task on IO thread.
  CONTENT_EXPORT void Stop();

 private:
  friend class base::RefCountedThreadSafe<DevToolsHttpProtocolHandler>;

  DevToolsHttpProtocolHandler(const std::string& ip,
                              int port,
                              const std::string& frontend_url,
                              Delegate* delegate);
  virtual ~DevToolsHttpProtocolHandler();
  void Start();

  // net::HttpServer::Delegate implementation.
  virtual void OnHttpRequest(int connection_id,
                             const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) OVERRIDE;
  virtual void OnClose(int connection_id) OVERRIDE;

  virtual void OnJsonRequestUI(int connection_id,
                               const net::HttpServerRequestInfo& info);
  virtual void OnWebSocketRequestUI(int connection_id,
                                    const net::HttpServerRequestInfo& info);
  virtual void OnWebSocketMessageUI(int connection_id,
                                    const std::string& data);
  virtual void OnCloseUI(int connection_id);

  // net::URLRequest::Delegate implementation.
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

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
                       const net::HttpServerRequestInfo& request);

  std::string ip_;
  int port_;
  std::string overridden_frontend_url_;
  scoped_refptr<net::HttpServer> server_;
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
  scoped_ptr<Delegate> delegate_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpProtocolHandler);
};

#endif  // CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HTTP_PROTOCOL_HANDLER_H_
