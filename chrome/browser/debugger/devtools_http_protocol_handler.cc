// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_http_protocol_handler.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
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
  virtual void InspectedTabClosing() {}
  virtual void SendMessageToClient(const IPC::Message& msg) {
    IPC_BEGIN_MESSAGE_MAP(DevToolsClientHostImpl, msg)
      IPC_MESSAGE_HANDLER(DevToolsClientMsg_RpcMessage, OnRpcMessage);
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP()
  }

 private:
  // Message handling routines
  void OnRpcMessage(const DevToolsMessageData& data) {
    std::string message;
    message += "devtools$$dispatch(\"" + data.class_name + "\", \"" +
        data.method_name + "\"";
    for (std::vector<std::string>::const_iterator it = data.arguments.begin();
         it != data.arguments.end(); ++it) {
      std::string param = *it;
      if (!param.empty())
        message += ", " + param;
    }
    message += ")";
    socket_->SendOverWebSocket(message);
  }
  HttpListenSocket* socket_;
};

}

DevToolsHttpProtocolHandler::~DevToolsHttpProtocolHandler() {
  // Stop() must be called prior to this being called
  DCHECK(server_.get() == NULL);
}

void DevToolsHttpProtocolHandler::Start() {
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DevToolsHttpProtocolHandler::Init));
}

void DevToolsHttpProtocolHandler::Stop() {
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DevToolsHttpProtocolHandler::Teardown));
}

void DevToolsHttpProtocolHandler::OnHttpRequest(HttpListenSocket* socket,
                                                HttpServerRequestInfo* info) {
  URLRequest* request = new URLRequest(GURL("chrome:/" + info->path), this);
  Bind(request, socket);
  request->set_context(
      Profile::GetDefaultRequestContext()->GetURLRequestContext());
  request->Start();
}

void DevToolsHttpProtocolHandler::OnWebSocketRequest(
    HttpListenSocket* socket,
    HttpServerRequestInfo* request) {
  socket->AcceptWebSocket(request);
}

void DevToolsHttpProtocolHandler::OnWebSocketMessage(HttpListenSocket* socket,
                                                     const std::string& data) {
  ChromeThread::PostTask(
      ChromeThread::UI,
      FROM_HERE,
      NewRunnableMethod(
          this,
          &DevToolsHttpProtocolHandler::OnWebSocketMessageUI,
          socket,
          data));
}

void DevToolsHttpProtocolHandler::OnWebSocketMessageUI(
    HttpListenSocket* socket,
    const std::string& d) {
  std::string data = d;
  if (!client_host_.get() && data == "attach") {
    client_host_.reset(new DevToolsClientHostImpl(socket));
    BrowserList::const_iterator it = BrowserList::begin();
    TabContents* tab_contents = (*it)->tabstrip_model()->GetTabContentsAt(0);
    DevToolsManager* manager = DevToolsManager::GetInstance();
    manager->RegisterDevToolsClientHostFor(tab_contents->render_view_host(),
                                           client_host_.get());
  } else {
    // TODO(pfeldman): Replace with proper parsing / dispatching.
    DevToolsMessageData message_data;
    message_data.class_name = "ToolsAgent";
    message_data.method_name = "dispatchOnInspectorController";

    size_t pos = data.find(" ");
    message_data.arguments.push_back(data.substr(0, pos));
    data = data.substr(pos + 1);

    pos = data.find(" ");
    message_data.arguments.push_back(data.substr(0, pos));
    data = data.substr(pos + 1);

    message_data.arguments.push_back(data);

    DevToolsManager* manager = DevToolsManager::GetInstance();
    manager->ForwardToDevToolsAgent(client_host_.get(),
        DevToolsAgentMsg_RpcMessage(DevToolsMessageData(message_data)));
  }
}

void DevToolsHttpProtocolHandler::OnClose(HttpListenSocket* socket) {
  SocketToRequestsMap::iterator it = socket_to_requests_.find(socket);
  if (it == socket_to_requests_.end())
    return;

  for (std::set<URLRequest*>::iterator it2 = it->second.begin();
       it2 != it->second.end(); ++it2) {
    URLRequest* request = *it2;
    request->Cancel();
    request_to_socket_.erase(request);
    request_to_buffer_.erase(request);
    delete request;
  }
  socket_to_requests_.erase(socket);
}

void DevToolsHttpProtocolHandler::OnResponseStarted(URLRequest* request) {
  RequestToSocketMap::iterator it = request_to_socket_.find(request);
  if (it == request_to_socket_.end())
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
    socket->Send("HTTP/1.1 404 Not Found\r\n"
                 "Content-Length: 0\r\n"
                 "\r\n");
  }

  int bytes_read = 0;
  // Some servers may treat HEAD requests as GET requests.  To free up the
  // network connection as soon as possible, signal that the request has
  // completed immediately, without trying to read any data back (all we care
  // about is the response code and headers, which we already have).
  net::IOBuffer* buffer = request_to_buffer_[request].get();
  if (request->status().is_success())
    request->Read(buffer, kBufferSize, &bytes_read);
  OnReadCompleted(request, bytes_read);
}

void DevToolsHttpProtocolHandler::OnReadCompleted(URLRequest* request,
                                                  int bytes_read) {
  RequestToSocketMap::iterator it = request_to_socket_.find(request);
  if (it == request_to_socket_.end())
    return;

  HttpListenSocket* socket = it->second;

  net::IOBuffer* buffer = request_to_buffer_[request].get();
  do {
    if (!request->status().is_success() || bytes_read <= 0)
      break;
    socket->Send(buffer->data(), bytes_read);
  } while (request->Read(buffer, kBufferSize, &bytes_read));

  // See comments re: HEAD requests in OnResponseStarted().
  if (!request->status().is_io_pending())
    RequestCompleted(request);
}

DevToolsHttpProtocolHandler::DevToolsHttpProtocolHandler(int port)
    : port_(port),
      server_(NULL) {
}

void DevToolsHttpProtocolHandler::Init() {
  server_ = HttpListenSocket::Listen("127.0.0.1", port_, this);
}

// Run on I/O thread
void DevToolsHttpProtocolHandler::Teardown() {
  server_ = NULL;
}

void DevToolsHttpProtocolHandler::Bind(URLRequest* request,
                                       HttpListenSocket* socket) {
  request_to_socket_[request] = socket;
  SocketToRequestsMap::iterator it = socket_to_requests_.find(socket);
  if (it == socket_to_requests_.end()) {
    std::pair<HttpListenSocket*, std::set<URLRequest*> > value(
        socket,
        std::set<URLRequest*>());
    it = socket_to_requests_.insert(value).first;
  }
  it->second.insert(request);
  request_to_buffer_[request] = new net::IOBuffer(kBufferSize);
}

void DevToolsHttpProtocolHandler::RequestCompleted(URLRequest* request) {
  RequestToSocketMap::iterator it = request_to_socket_.find(request);
  if (it == request_to_socket_.end())
    return;

  HttpListenSocket* socket = it->second;
  request_to_socket_.erase(request);
  SocketToRequestsMap::iterator it2 = socket_to_requests_.find(socket);
  it2->second.erase(request);
  request_to_buffer_.erase(request);
  delete request;
}
