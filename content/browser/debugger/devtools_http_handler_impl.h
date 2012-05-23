// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HTTP_HANDLER_IMPL_H_
#define CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HTTP_HANDLER_IMPL_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "net/server/http_server.h"
#include "net/url_request/url_request.h"

namespace net {
class StreamListenSocketFactory;
class URLRequestContextGetter;
}

namespace content {

class DevToolsClientHost;
class RenderViewHost;

class DevToolsHttpHandlerImpl
    : public DevToolsHttpHandler,
      public base::RefCountedThreadSafe<DevToolsHttpHandlerImpl>,
      public net::HttpServer::Delegate,
      public net::URLRequest::Delegate {
 private:
  struct PageInfo;
  typedef std::vector<PageInfo> PageList;
  friend class base::RefCountedThreadSafe<DevToolsHttpHandlerImpl>;
  friend class DevToolsHttpHandler;

  static bool SortPageListByTime(const PageInfo& info1, const PageInfo& info2);

  // Takes ownership over |socket_factory|.
  DevToolsHttpHandlerImpl(const net::StreamListenSocketFactory* socket_factory,
                          const std::string& frontend_url,
                          net::URLRequestContextGetter* request_context_getter,
                          DevToolsHttpHandlerDelegate* delegate);
  virtual ~DevToolsHttpHandlerImpl();
  void Start();

  // DevToolsHttpHandler implementation.
  virtual void Stop() OVERRIDE;
  virtual void SetRenderViewHostBinding(
      RenderViewHostBinding* binding) OVERRIDE;

  // net::HttpServer::Delegate implementation.
  virtual void OnHttpRequest(int connection_id,
                             const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) OVERRIDE;
  virtual void OnClose(int connection_id) OVERRIDE;

  PageList GeneratePageList();

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
  void TeardownAndRelease();
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

  std::string overridden_frontend_url_;
  scoped_ptr<const net::StreamListenSocketFactory> socket_factory_;
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
  typedef std::map<int, content::DevToolsClientHost*>
      ConnectionToClientHostMap;
  ConnectionToClientHostMap connection_to_client_host_ui_;
  net::URLRequestContextGetter* request_context_getter_;
  scoped_ptr<DevToolsHttpHandlerDelegate> delegate_;
  RenderViewHostBinding* binding_;
  scoped_ptr<RenderViewHostBinding> default_binding_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpHandlerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HTTP_HANDLER_IMPL_H_
