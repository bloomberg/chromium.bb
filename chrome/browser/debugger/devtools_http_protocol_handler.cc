// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_http_protocol_handler.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/server/http_server_request_info.h"
#include "net/url_request/url_request_context.h"

const int kBufferSize = 16 * 1024;

namespace {

// An internal implementation of DevToolsClientHost that delegates
// messages sent for DevToolsClient to a DebuggerShell instance.
class DevToolsClientHostImpl : public DevToolsClientHost {
 public:
  DevToolsClientHostImpl(
      HttpServer* server,
      int connection_id)
      : server_(server),
        connection_id_(connection_id) {
  }
  ~DevToolsClientHostImpl() {}

  // DevToolsClientHost interface
  virtual void InspectedTabClosing() {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableMethod(server_,
                          &HttpServer::Close,
                          connection_id_));
  }

  virtual void SendMessageToClient(const IPC::Message& msg) {
    IPC_BEGIN_MESSAGE_MAP(DevToolsClientHostImpl, msg)
      IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                          OnDispatchOnInspectorFrontend);
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP()
  }

  virtual void TabReplaced(TabContentsWrapper* new_tab) {
  }

  void NotifyCloseListener() {
    DevToolsClientHost::NotifyCloseListener();
  }
 private:
  // Message handling routines
  void OnDispatchOnInspectorFrontend(const std::string& data) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableMethod(server_,
                          &HttpServer::SendOverWebSocket,
                          connection_id_,
                          data));
  }

  virtual void FrameNavigating(const std::string& url) {}
  HttpServer* server_;
  int connection_id_;
};

}  // namespace


// static
scoped_refptr<DevToolsHttpProtocolHandler> DevToolsHttpProtocolHandler::Start(
    const std::string& ip,
    int port,
    const std::string& frontend_url,
    TabContentsProvider* provider) {
  scoped_refptr<DevToolsHttpProtocolHandler> http_handler =
      new DevToolsHttpProtocolHandler(ip, port, frontend_url, provider);
  http_handler->Start();
  return http_handler;
}

DevToolsHttpProtocolHandler::~DevToolsHttpProtocolHandler() {
  // Stop() must be called prior to this being called
  DCHECK(server_.get() == NULL);
}

void DevToolsHttpProtocolHandler::Start() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DevToolsHttpProtocolHandler::Init));
}

void DevToolsHttpProtocolHandler::Stop() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DevToolsHttpProtocolHandler::Teardown));
}

void DevToolsHttpProtocolHandler::OnHttpRequest(
    int connection_id,
    const HttpServerRequestInfo& info) {
  if (info.path == "" || info.path == "/") {
    // Pages discovery request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &DevToolsHttpProtocolHandler::OnHttpRequestUI,
                          connection_id,
                          info));
    return;
  }

  size_t pos = info.path.find("/devtools/");
  if (pos != 0) {
    server_->Send404(connection_id);
    return;
  }

  // Proxy static files from chrome-devtools://devtools/*.
  net::URLRequest* request = new net::URLRequest(
      GURL("chrome-devtools:/" + info.path), this);
  Bind(request, connection_id);
  request->set_context(
      Profile::GetDefaultRequestContext()->GetURLRequestContext());
  request->Start();
}

void DevToolsHttpProtocolHandler::OnWebSocketRequest(
    int connection_id,
    const HttpServerRequestInfo& request) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::OnWebSocketRequestUI,
          connection_id,
          request));
}

void DevToolsHttpProtocolHandler::OnWebSocketMessage(
    int connection_id,
    const std::string& data) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::OnWebSocketMessageUI,
          connection_id,
          data));
}

void DevToolsHttpProtocolHandler::OnClose(int connection_id) {
  ConnectionToRequestsMap::iterator it =
      connection_to_requests_io_.find(connection_id);
  if (it != connection_to_requests_io_.end()) {
    // Dispose delegating socket.
    for (std::set<net::URLRequest*>::iterator it2 = it->second.begin();
         it2 != it->second.end(); ++it2) {
      net::URLRequest* request = *it2;
      request->Cancel();
      request_to_connection_io_.erase(request);
      request_to_buffer_io_.erase(request);
      delete request;
    }
    connection_to_requests_io_.erase(connection_id);
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::OnCloseUI,
          connection_id));
}

void DevToolsHttpProtocolHandler::OnHttpRequestUI(
    int connection_id,
    const HttpServerRequestInfo& info) {
  std::string response = "<html><body><script>"
      "function addTab(id, url, attached, frontendUrl) {"
      "    if (!attached) {"
      "        var a = document.createElement('a');"
      "        a.textContent = url;"
      "        a.href = frontendUrl + '?host=' + window.location.host +"
      "            '&page=' + id;"
      "        document.body.appendChild(a);"
      "    } else {"
      "        var span = document.createElement('span');"
      "        span.textContent = url + ' (attached)';"
      "        document.body.appendChild(span);"
      "    }"
      "    document.body.appendChild(document.createElement('br'));"
      "}";

  InspectableTabs inspectable_tabs =
      tab_contents_provider_->GetInspectableTabs();

  for (InspectableTabs::iterator it = inspectable_tabs.begin();
       it != inspectable_tabs.end(); ++it) {

    TabContentsWrapper* tab_contents = *it;
    NavigationController& controller = tab_contents->controller();
    NavigationEntry* entry = controller.GetActiveEntry();
    if (entry == NULL)
      continue;

    if (!entry->url().is_valid())
      continue;

    DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
        GetDevToolsClientHostFor(tab_contents->tab_contents()->
                                      render_view_host());
    response += StringPrintf(
        "addTab(%d, '%s', %s, '%s');\n",
        controller.session_id().id(),
        entry->url().spec().c_str(),
        client_host ? "true" : "false",
        overriden_frontend_url_.c_str());
  }

  response += "</script></body></html>";
  Send200(connection_id, response, "text/html; charset=UTF-8");
}

void DevToolsHttpProtocolHandler::OnWebSocketRequestUI(
    int connection_id,
    const HttpServerRequestInfo& request) {
  std::string prefix = "/devtools/page/";
  size_t pos = request.path.find(prefix);
  if (pos != 0) {
    Send404(connection_id);
    return;
  }
  std::string page_id = request.path.substr(prefix.length());
  int id = 0;
  if (!base::StringToInt(page_id, &id)) {
    Send500(connection_id, "Invalid page id: " + page_id);
    return;
  }

  TabContents* tab_contents = GetTabContents(id);
  if (tab_contents == NULL) {
    Send500(connection_id, "No such page id: " + page_id);
    return;
  }

  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->GetDevToolsClientHostFor(tab_contents->render_view_host())) {
    Send500(connection_id, "Page with given id is being inspected: " + page_id);
    return;
  }

  DevToolsClientHostImpl* client_host =
      new DevToolsClientHostImpl(server_, connection_id);
  connection_to_client_host_ui_[connection_id] = client_host;

  manager->RegisterDevToolsClientHostFor(
      tab_contents->render_view_host(),
      client_host);
  manager->ForwardToDevToolsAgent(
      client_host,
      DevToolsAgentMsg_FrontendLoaded());

  AcceptWebSocket(connection_id, request);
}

void DevToolsHttpProtocolHandler::OnWebSocketMessageUI(
    int connection_id,
    const std::string& data) {
  ConnectionToClientHostMap::iterator it =
      connection_to_client_host_ui_.find(connection_id);
  if (it == connection_to_client_host_ui_.end())
    return;

  DevToolsManager* manager = DevToolsManager::GetInstance();

  // TODO(pfeldman): remove this once front-end stops sending it upstream.
  if (data == "loaded")
    return;

  manager->ForwardToDevToolsAgent(
      it->second,
      DevToolsAgentMsg_DispatchOnInspectorBackend(data));
}

void DevToolsHttpProtocolHandler::OnCloseUI(int connection_id) {
  ConnectionToClientHostMap::iterator it =
      connection_to_client_host_ui_.find(connection_id);
  if (it != connection_to_client_host_ui_.end()) {
    DevToolsClientHostImpl* client_host =
        static_cast<DevToolsClientHostImpl*>(it->second);
    client_host->NotifyCloseListener();
    delete client_host;
    connection_to_client_host_ui_.erase(connection_id);
  }
}

void DevToolsHttpProtocolHandler::OnResponseStarted(net::URLRequest* request) {
  RequestToSocketMap::iterator it = request_to_connection_io_.find(request);
  if (it == request_to_connection_io_.end())
    return;

  int connection_id = it->second;

  int expected_size = static_cast<int>(request->GetExpectedContentSize());

  std::string content_type;
  request->GetMimeType(&content_type);

  if (request->status().is_success()) {
    server_->Send(connection_id, StringPrintf("HTTP/1.1 200 OK\r\n"
                                              "Content-Type:%s\r\n"
                                              "Content-Length:%d\r\n"
                                              "\r\n",
                                              content_type.c_str(),
                                              expected_size));
  } else {
    server_->Send404(connection_id);
  }

  int bytes_read = 0;
  // Some servers may treat HEAD requests as GET requests.  To free up the
  // network connection as soon as possible, signal that the request has
  // completed immediately, without trying to read any data back (all we care
  // about is the response code and headers, which we already have).
  net::IOBuffer* buffer = request_to_buffer_io_[request].get();
  if (request->status().is_success())
    request->Read(buffer, kBufferSize, &bytes_read);
  OnReadCompleted(request, bytes_read);
}

void DevToolsHttpProtocolHandler::OnReadCompleted(net::URLRequest* request,
                                                  int bytes_read) {
  RequestToSocketMap::iterator it = request_to_connection_io_.find(request);
  if (it == request_to_connection_io_.end())
    return;

  int connection_id = it->second;

  net::IOBuffer* buffer = request_to_buffer_io_[request].get();
  do {
    if (!request->status().is_success() || bytes_read <= 0)
      break;
    server_->Send(connection_id, buffer->data(), bytes_read);
  } while (request->Read(buffer, kBufferSize, &bytes_read));

  // See comments re: HEAD requests in OnResponseStarted().
  if (!request->status().is_io_pending())
    RequestCompleted(request);
}

DevToolsHttpProtocolHandler::DevToolsHttpProtocolHandler(
    const std::string& ip,
    int port,
    const std::string& frontend_host,
    TabContentsProvider* provider)
    : ip_(ip),
      port_(port),
      overriden_frontend_url_(frontend_host),
      tab_contents_provider_(provider) {
  if (overriden_frontend_url_.empty())
      overriden_frontend_url_ = "/devtools/devtools.html";
}

void DevToolsHttpProtocolHandler::Init() {
  server_ = new HttpServer(ip_, port_, this);
}

// Run on I/O thread
void DevToolsHttpProtocolHandler::Teardown() {
  server_ = NULL;
}

void DevToolsHttpProtocolHandler::Bind(net::URLRequest* request,
                                       int connection_id) {
  request_to_connection_io_[request] = connection_id;
  ConnectionToRequestsMap::iterator it =
      connection_to_requests_io_.find(connection_id);
  if (it == connection_to_requests_io_.end()) {
    std::pair<int, std::set<net::URLRequest*> > value(
        connection_id,
        std::set<net::URLRequest*>());
    it = connection_to_requests_io_.insert(value).first;
  }
  it->second.insert(request);
  request_to_buffer_io_[request] = new net::IOBuffer(kBufferSize);
}

void DevToolsHttpProtocolHandler::RequestCompleted(net::URLRequest* request) {
  RequestToSocketMap::iterator it = request_to_connection_io_.find(request);
  if (it == request_to_connection_io_.end())
    return;

  int connection_id = it->second;
  request_to_connection_io_.erase(request);
  ConnectionToRequestsMap::iterator it2 =
      connection_to_requests_io_.find(connection_id);
  it2->second.erase(request);
  request_to_buffer_io_.erase(request);
  delete request;
}

void DevToolsHttpProtocolHandler::Send200(int connection_id,
                                          const std::string& data,
                                          const std::string& mime_type) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(server_.get(),
                        &HttpServer::Send200,
                        connection_id,
                        data,
                        mime_type));
}

void DevToolsHttpProtocolHandler::Send404(int connection_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(server_.get(),
                        &HttpServer::Send404,
                        connection_id));
}

void DevToolsHttpProtocolHandler::Send500(int connection_id,
                                          const std::string& message) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(server_.get(),
                        &HttpServer::Send500,
                        connection_id,
                        message));
}

void DevToolsHttpProtocolHandler::AcceptWebSocket(
    int connection_id,
    const HttpServerRequestInfo& request) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(server_.get(),
                        &HttpServer::AcceptWebSocket,
                        connection_id,
                        request));
}

TabContents* DevToolsHttpProtocolHandler::GetTabContents(int session_id) {
  InspectableTabs inspectable_tabs =
      tab_contents_provider_->GetInspectableTabs();

  for (InspectableTabs::iterator it = inspectable_tabs.begin();
       it != inspectable_tabs.end(); ++it) {
    TabContentsWrapper* tab_contents = *it;
    NavigationController& controller =
        tab_contents->controller();
    if (controller.session_id().id() == session_id)
      return controller.tab_contents();
  }
  return NULL;
}
