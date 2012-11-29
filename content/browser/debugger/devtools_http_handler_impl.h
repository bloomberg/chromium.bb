// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HTTP_HANDLER_IMPL_H_
#define CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HTTP_HANDLER_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/server/http_server.h"

namespace base {
class DictionaryValue;
class Thread;
class Value;
}

namespace net {
class StreamListenSocketFactory;
class URLRequestContextGetter;
}

namespace content {

class DevToolsClientHost;
class RenderViewHost;

class DevToolsHttpHandlerImpl
    : public DevToolsHttpHandler,
      public NotificationObserver,
      public base::RefCountedThreadSafe<DevToolsHttpHandlerImpl>,
      public net::HttpServer::Delegate {
 private:
  struct PageInfo;
  typedef std::vector<PageInfo> PageList;
  friend class base::RefCountedThreadSafe<DevToolsHttpHandlerImpl>;
  friend class DevToolsHttpHandler;

  static bool SortPageListByTime(const PageInfo& info1, const PageInfo& info2);

  // Takes ownership over |socket_factory|.
  DevToolsHttpHandlerImpl(const net::StreamListenSocketFactory* socket_factory,
                          const std::string& frontend_url,
                          DevToolsHttpHandlerDelegate* delegate);
  virtual ~DevToolsHttpHandlerImpl();
  void Start();

  // DevToolsHttpHandler implementation.
  virtual void Stop() OVERRIDE;
  virtual void SetRenderViewHostBinding(
      RenderViewHostBinding* binding) OVERRIDE;
  virtual GURL GetFrontendURL(RenderViewHost* render_view_host) OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // net::HttpServer::Delegate implementation.
  virtual void OnHttpRequest(int connection_id,
                             const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) OVERRIDE;
  virtual void OnClose(int connection_id) OVERRIDE;

  void OnVersionRequestUI(int connection_id,
                          const net::HttpServerRequestInfo& info);
  void OnJsonRequestUI(int connection_id,
                       const net::HttpServerRequestInfo& info);
  void OnNewTargetRequestUI(int connection_id,
                            const net::HttpServerRequestInfo& info);
  void OnCloseTargetRequestUI(int connection_id,
                              const net::HttpServerRequestInfo& info);
  void OnThumbnailRequestUI(int connection_id,
                       const net::HttpServerRequestInfo& info);
  void OnDiscoveryPageRequestUI(int connection_id);

  void OnWebSocketRequestUI(int connection_id,
                            const net::HttpServerRequestInfo& info);
  void OnWebSocketMessageUI(int connection_id, const std::string& data);
  void OnCloseUI(int connection_id);

  void ResetHandlerThread();
  void ResetHandlerThreadAndRelease();

  void Init();
  void Teardown();

  void StartHandlerThread();
  void StopHandlerThread();

  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type = "text/html");
  void SendJson(int connection_id,
                const net::HttpServerRequestInfo& info,
                const base::Value& value);
  void Send404(int connection_id);
  void Send500(int connection_id,
               const std::string& message);
  void AcceptWebSocket(int connection_id,
                       const net::HttpServerRequestInfo& request);

  PageList GeneratePageList();

  // Returns the front end url without the host at the beginning.
  std::string GetFrontendURLInternal(const std::string rvh_id,
                                     const std::string& host);

  PageInfo CreatePageInfo(RenderViewHost* rvh);

  base::DictionaryValue* SerializePageInfo(const PageInfo& page_info,
                                           const std::string& host);

  // The thread used by the devtools handler to run server socket.
  scoped_ptr<base::Thread> thread_;

  std::string overridden_frontend_url_;
  scoped_ptr<const net::StreamListenSocketFactory> socket_factory_;
  scoped_refptr<net::HttpServer> server_;
  typedef std::map<int, DevToolsClientHost*> ConnectionToClientHostMap;
  ConnectionToClientHostMap connection_to_client_host_ui_;
  scoped_ptr<DevToolsHttpHandlerDelegate> delegate_;
  RenderViewHostBinding* binding_;
  scoped_ptr<RenderViewHostBinding> default_binding_;
  NotificationRegistrar registrar_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpHandlerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEBUGGER_DEVTOOLS_HTTP_HANDLER_IMPL_H_
