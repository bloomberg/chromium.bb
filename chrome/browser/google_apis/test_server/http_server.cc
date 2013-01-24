// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/test_server/http_server.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/google_apis/test_server/http_connection.h"
#include "chrome/browser/google_apis/test_server/http_request.h"
#include "chrome/browser/google_apis/test_server/http_response.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "net/tools/fetch/http_listen_socket.h"

namespace google_apis {
namespace test_server {

using content::BrowserThread;

namespace {

const int kPort = 8040;
const char kIp[] = "127.0.0.1";
const int kRetries = 10;

// Callback to handle requests with default predefined response for requests
// matching the address |url|.
scoped_ptr<HttpResponse> HandleDefaultRequest(const GURL& url,
                                              const HttpResponse& response,
                                              const HttpRequest& request) {
  const GURL request_url = url.Resolve(request.relative_url);
  if (url.path() != request_url.path())
    return scoped_ptr<HttpResponse>(NULL);
  return scoped_ptr<HttpResponse>(new HttpResponse(response));
}

}  // namespace

HttpListenSocket::HttpListenSocket(const SocketDescriptor socket_descriptor,
                                   net::StreamListenSocket::Delegate* delegate)
    : net::TCPListenSocket(socket_descriptor, delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void HttpListenSocket::Listen() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::TCPListenSocket::Listen();
}

HttpListenSocket::~HttpListenSocket() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

HttpServer::HttpServer()
    : port_(-1),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

HttpServer::~HttpServer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool HttpServer::InitializeAndWaitUntilReady() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&HttpServer::InitializeOnIOThread,
                 base::Unretained(this)));

  // Wait for the task completion.
  content::RunAllPendingInMessageLoop(BrowserThread::IO);

  return Started();
}

void HttpServer::ShutdownAndWaitUntilComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&HttpServer::ShutdownOnIOThread,
                 base::Unretained(this)));

  // Wait for the task completion.
  content::RunAllPendingInMessageLoop(BrowserThread::IO);
}

void HttpServer::InitializeOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!Started());

  int retries_left = kRetries + 1;
  int try_port = kPort;

  while (retries_left > 0) {
    SocketDescriptor socket_descriptor = net::TCPListenSocket::CreateAndBind(
        kIp,
        try_port);
    if (socket_descriptor != net::TCPListenSocket::kInvalidSocket) {
      listen_socket_ = new HttpListenSocket(socket_descriptor, this);
      listen_socket_->Listen();
      base_url_ = GURL(base::StringPrintf("http://%s:%d", kIp, try_port));
      port_ = try_port;
      break;
    }
    retries_left--;
    try_port++;
  }
}

void HttpServer::ShutdownOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  listen_socket_ = NULL;  // Release the listen socket.
  STLDeleteContainerPairSecondPointers(connections_.begin(),
                                       connections_.end());
  connections_.clear();
}

void HttpServer::HandleRequest(HttpConnection* connection,
                               scoped_ptr<HttpRequest> request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (size_t i = 0; i < request_handlers_.size(); ++i) {
    scoped_ptr<HttpResponse> response =
        request_handlers_[i].Run(*request.get());
    if (response.get()) {
      connection->SendResponse(response.Pass());
      return;
    }
  }

  LOG(WARNING) << "Request not handled. Returning 404: "
               << request->relative_url;
  scoped_ptr<HttpResponse> not_found_response(new HttpResponse());
  not_found_response->set_code(NOT_FOUND);
  connection->SendResponse(not_found_response.Pass());

  // Drop the connection, since we do not support multiple requests per
  // connection.
  connections_.erase(connection->socket_.get());
  delete connection;
}

GURL HttpServer::GetURL(const std::string& relative_url) const {
  DCHECK(StartsWithASCII(relative_url, "/", true /* case_sensitive */))
      << relative_url;
  return base_url_.Resolve(relative_url);
}

void HttpServer::RegisterRequestHandler(
    const HandleRequestCallback& callback) {
  request_handlers_.push_back(callback);
}

void HttpServer::DidAccept(net::StreamListenSocket* server,
                           net::StreamListenSocket* connection) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  HttpConnection* http_connection = new HttpConnection(
      connection,
      base::Bind(&HttpServer::HandleRequest, weak_factory_.GetWeakPtr()));
  connections_[connection] = http_connection;
}

void HttpServer::DidRead(net::StreamListenSocket* connection,
                         const char* data,
                         int length) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  HttpConnection* http_connection = FindConnection(connection);
  if (http_connection == NULL) {
    LOG(WARNING) << "Unknown connection.";
    return;
  }
  http_connection->ReceiveData(std::string(data, length));
}

void HttpServer::DidClose(net::StreamListenSocket* connection) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  HttpConnection* http_connection = FindConnection(connection);
  if (http_connection == NULL) {
    LOG(WARNING) << "Unknown connection.";
    return;
  }
  delete http_connection;
  connections_.erase(connection);
}

HttpConnection* HttpServer::FindConnection(
    net::StreamListenSocket* socket) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::map<net::StreamListenSocket*, HttpConnection*>::iterator it =
      connections_.find(socket);
  if (it == connections_.end()) {
    return NULL;
  }
  return it->second;
}

}  // namespace test_server
}  // namespace google_apis
