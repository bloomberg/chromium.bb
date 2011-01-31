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
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/listen_socket.h"
#include "net/server/http_server_request_info.h"
#include "net/url_request/url_request_context.h"

const int kBufferSize = 16 * 1024;

namespace {

// An internal implementation of DevToolsClientHost that delegates
// messages sent for DevToolsClient to a DebuggerShell instance.
class DevToolsClientHostImpl : public DevToolsClientHost {
 public:
  explicit DevToolsClientHostImpl(HttpListenSocket* socket)
      : socket_(socket) {}
  ~DevToolsClientHostImpl() {}

  // DevToolsClientHost interface
  virtual void InspectedTabClosing() {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableMethod(socket_,
                          &HttpListenSocket::Close));
  }

  virtual void SendMessageToClient(const IPC::Message& msg) {
    IPC_BEGIN_MESSAGE_MAP(DevToolsClientHostImpl, msg)
      IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                          OnDispatchOnInspectorFrontend);
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP()
  }

  void NotifyCloseListener() {
    DevToolsClientHost::NotifyCloseListener();
  }
 private:
  // Message handling routines
  void OnDispatchOnInspectorFrontend(const std::string& data) {
    socket_->SendOverWebSocket(data);
  }
  virtual void FrameNavigating(const std::string& url) {}
  HttpListenSocket* socket_;
};

}  // namespace


// static
scoped_refptr<DevToolsHttpProtocolHandler> DevToolsHttpProtocolHandler::Start(
    int port,
    const std::string& frontend_url,
    TabContentsProvider* provider) {
  scoped_refptr<DevToolsHttpProtocolHandler> http_handler =
      new DevToolsHttpProtocolHandler(port, frontend_url, provider);
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
    HttpListenSocket* socket,
    const HttpServerRequestInfo& info) {
  if (info.path == "" || info.path == "/") {
    // Pages discovery request.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &DevToolsHttpProtocolHandler::OnHttpRequestUI,
                          make_scoped_refptr(socket),
                          info));
    return;
  }

  size_t pos = info.path.find("/devtools/");
  if (pos != 0) {
    socket->Send404();
    return;
  }

  // Proxy static files from chrome://devtools/*.
  net::URLRequest* request = new net::URLRequest(
      GURL("chrome:/" + info.path), this);
  Bind(request, socket);
  request->set_context(
      Profile::GetDefaultRequestContext()->GetURLRequestContext());
  request->Start();
}

void DevToolsHttpProtocolHandler::OnWebSocketRequest(
    HttpListenSocket* socket,
    const HttpServerRequestInfo& request) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::OnWebSocketRequestUI,
          make_scoped_refptr(socket),
          request));
}

void DevToolsHttpProtocolHandler::OnWebSocketMessage(HttpListenSocket* socket,
                                                     const std::string& data) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::OnWebSocketMessageUI,
          make_scoped_refptr(socket),
          data));
}

void DevToolsHttpProtocolHandler::OnClose(HttpListenSocket* socket) {
  SocketToRequestsMap::iterator it = socket_to_requests_io_.find(socket);
  if (it != socket_to_requests_io_.end()) {
    // Dispose delegating socket.
    for (std::set<net::URLRequest*>::iterator it2 = it->second.begin();
         it2 != it->second.end(); ++it2) {
      net::URLRequest* request = *it2;
      request->Cancel();
      request_to_socket_io_.erase(request);
      request_to_buffer_io_.erase(request);
      delete request;
    }
    socket_to_requests_io_.erase(socket);
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::OnCloseUI,
          make_scoped_refptr(socket)));
}

void DevToolsHttpProtocolHandler::OnHttpRequestUI(
    HttpListenSocket* socket,
    const HttpServerRequestInfo& info) {
  std::string response = "<html><body><script>"
      "function addTab(id, url, attached, frontendUrl) {"
      "    if (!attached) {"
      "        var a = document.createElement('a');"
      "        a.textContent = url;"
      "        a.href = frontendUrl + '?remotehost=' + window.location.host +"
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
  Send200(socket, response, "text/html; charset=UTF-8");
}

void DevToolsHttpProtocolHandler::OnWebSocketRequestUI(
    HttpListenSocket* socket,
    const HttpServerRequestInfo& request) {
  std::string prefix = "/devtools/page/";
  size_t pos = request.path.find(prefix);
  if (pos != 0) {
    Send404(socket);
    return;
  }
  std::string page_id = request.path.substr(prefix.length());
  int id = 0;
  if (!base::StringToInt(page_id, &id)) {
    Send500(socket, "Invalid page id: " + page_id);
    return;
  }

  TabContents* tab_contents = GetTabContents(id);
  if (tab_contents == NULL) {
    Send500(socket, "No such page id: " + page_id);
    return;
  }

  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (manager->GetDevToolsClientHostFor(tab_contents->render_view_host())) {
    Send500(socket, "Page with given id is being inspected: " + page_id);
    return;
  }

  DevToolsClientHostImpl* client_host = new DevToolsClientHostImpl(socket);
  socket_to_client_host_ui_[socket] = client_host;

  manager->RegisterDevToolsClientHostFor(
      tab_contents->render_view_host(),
      client_host);
  AcceptWebSocket(socket, request);
}

void DevToolsHttpProtocolHandler::OnWebSocketMessageUI(
    HttpListenSocket* socket,
    const std::string& data) {
  SocketToClientHostMap::iterator it = socket_to_client_host_ui_.find(socket);
  if (it == socket_to_client_host_ui_.end())
    return;

  DevToolsManager* manager = DevToolsManager::GetInstance();

  if (data == "loaded") {
    manager->ForwardToDevToolsAgent(
        it->second,
        DevToolsAgentMsg_FrontendLoaded());
    return;
  }

  manager->ForwardToDevToolsAgent(
      it->second,
      DevToolsAgentMsg_DispatchOnInspectorBackend(data));
}

void DevToolsHttpProtocolHandler::OnCloseUI(HttpListenSocket* socket) {
  SocketToClientHostMap::iterator it = socket_to_client_host_ui_.find(socket);
  if (it != socket_to_client_host_ui_.end()) {
    DevToolsClientHostImpl* client_host =
        static_cast<DevToolsClientHostImpl*>(it->second);
    client_host->NotifyCloseListener();
    delete client_host;
    socket_to_client_host_ui_.erase(socket);
  }

  // We are holding last reference to scoped refptr 'socket' here.
  // We can't exit method just like that since 'socket' is going to
  // be destroyed on the UI thread then. Schedule no-op to IO thread
  // so that socket is destroyed on IO instead.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::ReleaseSocket,
          make_scoped_refptr(socket)));
}

void DevToolsHttpProtocolHandler::OnResponseStarted(net::URLRequest* request) {
  RequestToSocketMap::iterator it = request_to_socket_io_.find(request);
  if (it == request_to_socket_io_.end())
    return;

  HttpListenSocket* socket = it->second;

  int expected_size = static_cast<int>(request->GetExpectedContentSize());

  std::string content_type;
  request->GetMimeType(&content_type);

  if (request->status().is_success()) {
    socket->Send(StringPrintf("HTTP/1.1 200 OK\r\n"
                              "Content-Type:%s\r\n"
                              "Content-Length:%d\r\n"
                              "\r\n",
                              content_type.c_str(),
                              expected_size));
  } else {
    socket->Send404();
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
  RequestToSocketMap::iterator it = request_to_socket_io_.find(request);
  if (it == request_to_socket_io_.end())
    return;

  HttpListenSocket* socket = it->second;

  net::IOBuffer* buffer = request_to_buffer_io_[request].get();
  do {
    if (!request->status().is_success() || bytes_read <= 0)
      break;
    socket->Send(buffer->data(), bytes_read);
  } while (request->Read(buffer, kBufferSize, &bytes_read));

  // See comments re: HEAD requests in OnResponseStarted().
  if (!request->status().is_io_pending())
    RequestCompleted(request);
}

DevToolsHttpProtocolHandler::DevToolsHttpProtocolHandler(
    int port,
    const std::string& frontend_host,
    TabContentsProvider* provider)
    : port_(port),
      overriden_frontend_url_(frontend_host),
      server_(NULL),
      tab_contents_provider_(provider) {
  if (overriden_frontend_url_.empty())
      overriden_frontend_url_ = "/devtools/devtools.html";
}

void DevToolsHttpProtocolHandler::Init() {
  server_ = HttpListenSocket::Listen("127.0.0.1", port_, this);
}

// Run on I/O thread
void DevToolsHttpProtocolHandler::Teardown() {
  server_ = NULL;
}

void DevToolsHttpProtocolHandler::Bind(net::URLRequest* request,
                                       HttpListenSocket* socket) {
  request_to_socket_io_[request] = socket;
  SocketToRequestsMap::iterator it = socket_to_requests_io_.find(socket);
  if (it == socket_to_requests_io_.end()) {
    std::pair<HttpListenSocket*, std::set<net::URLRequest*> > value(
        socket,
        std::set<net::URLRequest*>());
    it = socket_to_requests_io_.insert(value).first;
  }
  it->second.insert(request);
  request_to_buffer_io_[request] = new net::IOBuffer(kBufferSize);
}

void DevToolsHttpProtocolHandler::RequestCompleted(net::URLRequest* request) {
  RequestToSocketMap::iterator it = request_to_socket_io_.find(request);
  if (it == request_to_socket_io_.end())
    return;

  HttpListenSocket* socket = it->second;
  request_to_socket_io_.erase(request);
  SocketToRequestsMap::iterator it2 = socket_to_requests_io_.find(socket);
  it2->second.erase(request);
  request_to_buffer_io_.erase(request);
  delete request;
}

void DevToolsHttpProtocolHandler::Send200(HttpListenSocket* socket,
                                          const std::string& data,
                                          const std::string& mime_type) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(socket,
                        &HttpListenSocket::Send200,
                        data,
                        mime_type));
}

void DevToolsHttpProtocolHandler::Send404(HttpListenSocket* socket) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(socket,
                        &HttpListenSocket::Send404));
}

void DevToolsHttpProtocolHandler::Send500(HttpListenSocket* socket,
                                          const std::string& message) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(socket,
                        &HttpListenSocket::Send500,
                        message));
}

void DevToolsHttpProtocolHandler::AcceptWebSocket(
    HttpListenSocket* socket,
    const HttpServerRequestInfo& request) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(socket,
                        &HttpListenSocket::AcceptWebSocket,
                        request));
}

void DevToolsHttpProtocolHandler::ReleaseSocket(
    HttpListenSocket* socket) {
  // This in fact is scoped ref ptr. It'll get nuked on exit.
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
