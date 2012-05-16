// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <objbase.h>
#include <urlmon.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome_frame/test/test_server.h"
#include "net/base/tcp_listen_socket.h"
#include "net/base/winsock_init.h"
#include "net/http/http_util.h"

namespace test_server {
const char kDefaultHeaderTemplate[] =
    "HTTP/1.1 %hs\r\n"
    "Connection: close\r\n"
    "Content-Type: %hs\r\n"
    "Content-Length: %i\r\n\r\n";
const char kStatusOk[] = "200 OK";
const char kStatusNotFound[] = "404 Not Found";
const char kDefaultContentType[] = "text/html; charset=UTF-8";

void Request::ParseHeaders(const std::string& headers) {
  DCHECK(method_.length() == 0);

  size_t pos = headers.find("\r\n");
  DCHECK(pos != std::string::npos);
  if (pos != std::string::npos) {
    headers_ = headers.substr(pos + 2);

    StringTokenizer tokenizer(headers.begin(), headers.begin() + pos, " ");
    std::string* parse[] = { &method_, &path_, &version_ };
    int field = 0;
    while (tokenizer.GetNext() && field < arraysize(parse)) {
      parse[field++]->assign(tokenizer.token_begin(),
                             tokenizer.token_end());
    }
  }

  // Check for content-length in case we're being sent some data.
  net::HttpUtil::HeadersIterator it(headers_.begin(), headers_.end(),
                                    "\r\n");
  while (it.GetNext()) {
    if (LowerCaseEqualsASCII(it.name(), "content-length")) {
      int int_content_length;
      base::StringToInt(base::StringPiece(it.values_begin(),
                                          it.values_end()),
                        &int_content_length);
      content_length_ = int_content_length;
      break;
    }
  }
}

void Request::OnDataReceived(const std::string& data) {
  content_ += data;

  if (method_.length() == 0) {
    size_t index = content_.find("\r\n\r\n");
    if (index != std::string::npos) {
      // Parse the headers before returning and chop them of the
      // data buffer we've already received.
      std::string headers(content_.substr(0, index + 2));
      ParseHeaders(headers);
      content_.erase(0, index + 4);
    }
  }
}

ResponseForPath::~ResponseForPath() {
}

SimpleResponse::~SimpleResponse() {
}

bool FileResponse::GetContentType(std::string* content_type) const {
  size_t length = ContentLength();
  char buffer[4096];
  void* data = NULL;

  if (length) {
    // Create a copy of the first few bytes of the file.
    // If we try and use the mapped file directly, FindMimeFromData will crash
    // 'cause it cheats and temporarily tries to write to the buffer!
    length = std::min(arraysize(buffer), length);
    memcpy(buffer, file_->data(), length);
    data = buffer;
  }

  LPOLESTR mime_type = NULL;
  FindMimeFromData(NULL, file_path_.value().c_str(), data, length, NULL,
                   FMFD_DEFAULT, &mime_type, 0);
  if (mime_type) {
    *content_type = WideToASCII(mime_type);
    ::CoTaskMemFree(mime_type);
  }

  return content_type->length() > 0;
}

void FileResponse::WriteContents(net::StreamListenSocket* socket) const {
  DCHECK(file_.get());
  if (file_.get()) {
    socket->Send(reinterpret_cast<const char*>(file_->data()),
                 file_->length(), false);
  }
}

size_t FileResponse::ContentLength() const {
  if (file_.get() == NULL) {
    file_.reset(new file_util::MemoryMappedFile());
    if (!file_->Initialize(file_path_)) {
      NOTREACHED();
      file_.reset();
    }
  }
  return file_.get() ? file_->length() : 0;
}

bool RedirectResponse::GetCustomHeaders(std::string* headers) const {
  *headers = base::StringPrintf("HTTP/1.1 302 Found\r\n"
                                "Connection: close\r\n"
                                "Content-Length: 0\r\n"
                                "Content-Type: text/html\r\n"
                                "Location: %hs\r\n\r\n",
                                redirect_url_.c_str());
  return true;
}

SimpleWebServer::SimpleWebServer(int port) {
  CHECK(MessageLoop::current()) << "SimpleWebServer requires a message loop";
  net::EnsureWinsockInit();
  AddResponse(&quit_);
  server_ = net::TCPListenSocket::CreateAndListen("127.0.0.1", port, this);
  DCHECK(server_.get() != NULL);
}

SimpleWebServer::~SimpleWebServer() {
  ConnectionList::const_iterator it;
  for (it = connections_.begin(); it != connections_.end(); ++it)
    delete (*it);
  connections_.clear();
}

void SimpleWebServer::AddResponse(Response* response) {
  responses_.push_back(response);
}

void SimpleWebServer::DeleteAllResponses() {
  std::list<Response*>::const_iterator it;
  for (it = responses_.begin(); it != responses_.end(); ++it) {
    if ((*it) != &quit_)
      delete (*it);
  }
}

Response* SimpleWebServer::FindResponse(const Request& request) const {
  std::list<Response*>::const_iterator it;
  for (it = responses_.begin(); it != responses_.end(); it++) {
    Response* response = (*it);
    if (response->Matches(request)) {
      return response;
    }
  }
  return NULL;
}

Connection* SimpleWebServer::FindConnection(
    const net::StreamListenSocket* socket) const {
  ConnectionList::const_iterator it;
  for (it = connections_.begin(); it != connections_.end(); it++) {
    if ((*it)->IsSame(socket)) {
      return (*it);
    }
  }
  return NULL;
}

void SimpleWebServer::DidAccept(net::StreamListenSocket* server,
                                net::StreamListenSocket* connection) {
  connections_.push_back(new Connection(connection));
}

void SimpleWebServer::DidRead(net::StreamListenSocket* connection,
                              const char* data,
                              int len) {
  Connection* c = FindConnection(connection);
  DCHECK(c);
  Request& r = c->request();
  std::string str(data, len);
  r.OnDataReceived(str);
  if (r.AllContentReceived()) {
    const Request& request = c->request();
    Response* response = FindResponse(request);
    if (response) {
      std::string headers;
      if (!response->GetCustomHeaders(&headers)) {
        std::string content_type;
        if (!response->GetContentType(&content_type))
          content_type = kDefaultContentType;
        headers = base::StringPrintf(kDefaultHeaderTemplate, kStatusOk,
                                     content_type.c_str(),
                                     response->ContentLength());
      }

      connection->Send(headers, false);
      response->WriteContents(connection);
      response->IncrementAccessCounter();
    } else {
      std::string payload = "sorry, I can't find " + request.path();
      std::string headers(base::StringPrintf(kDefaultHeaderTemplate,
                                             kStatusNotFound,
                                             kDefaultContentType,
                                             payload.length()));
      connection->Send(headers, false);
      connection->Send(payload, false);
    }
  }
}

void SimpleWebServer::DidClose(net::StreamListenSocket* sock) {
  // To keep the historical list of connections reasonably tidy, we delete
  // 404's when the connection ends.
  Connection* c = FindConnection(sock);
  DCHECK(c);
  c->OnSocketClosed();
  if (!FindResponse(c->request())) {
    // extremely inefficient, but in one line and not that common... :)
    connections_.erase(std::find(connections_.begin(), connections_.end(), c));
    delete c;
  }
}

HTTPTestServer::HTTPTestServer(int port, const std::wstring& address,
                               FilePath root_dir)
    : port_(port), address_(address), root_dir_(root_dir) {
  net::EnsureWinsockInit();
  server_ =
      net::TCPListenSocket::CreateAndListen(WideToUTF8(address), port, this);
}

HTTPTestServer::~HTTPTestServer() {
  server_ = NULL;
}

std::list<scoped_refptr<ConfigurableConnection>>::iterator
HTTPTestServer::FindConnection(const net::StreamListenSocket* socket) {
  ConnectionList::iterator it;
  // Scan through the list searching for the desired socket. Along the way,
  // erase any connections for which the corresponding socket has already been
  // forgotten about as a result of all data having been sent.
  for (it = connection_list_.begin(); it != connection_list_.end(); ) {
    ConfigurableConnection* connection = it->get();
    if (connection->socket_ == NULL) {
      connection_list_.erase(it++);
      continue;
    }
    if (connection->socket_ == socket)
      break;
    ++it;
  }

  return it;
}

scoped_refptr<ConfigurableConnection> HTTPTestServer::ConnectionFromSocket(
    const net::StreamListenSocket* socket) {
  ConnectionList::iterator it = FindConnection(socket);
  if (it != connection_list_.end())
    return *it;
  return NULL;
}

void HTTPTestServer::DidAccept(net::StreamListenSocket* server,
                               net::StreamListenSocket* socket) {
  connection_list_.push_back(new ConfigurableConnection(socket));
}

void HTTPTestServer::DidRead(net::StreamListenSocket* socket,
                             const char* data,
                             int len) {
  scoped_refptr<ConfigurableConnection> connection =
      ConnectionFromSocket(socket);
  if (connection) {
    std::string str(data, len);
    connection->r_.OnDataReceived(str);
    if (connection->r_.AllContentReceived()) {
      VLOG(1) << __FUNCTION__ << ": " << connection->r_.method() << " "
              << connection->r_.path();
      std::wstring path = UTF8ToWide(connection->r_.path());
      if (LowerCaseEqualsASCII(connection->r_.method(), "post"))
        this->Post(connection, path, connection->r_);
      else
        this->Get(connection, path, connection->r_);
    }
  }
}

void HTTPTestServer::DidClose(net::StreamListenSocket* socket) {
  ConnectionList::iterator it = FindConnection(socket);
  if (it != connection_list_.end())
    connection_list_.erase(it);
}

std::wstring HTTPTestServer::Resolve(const std::wstring& path) {
  // Remove the first '/' if needed.
  std::wstring stripped_path = path;
  if (path.size() && path[0] == L'/')
    stripped_path = path.substr(1);

  if (port_ == 80) {
    if (stripped_path.empty()) {
      return base::StringPrintf(L"http://%ls", address_.c_str());
    } else {
      return base::StringPrintf(L"http://%ls/%ls", address_.c_str(),
                          stripped_path.c_str());
    }
  } else {
    if (stripped_path.empty()) {
      return base::StringPrintf(L"http://%ls:%d", address_.c_str(), port_);
    } else {
      return base::StringPrintf(L"http://%ls:%d/%ls", address_.c_str(), port_,
                                stripped_path.c_str());
    }
  }
}

void ConfigurableConnection::SendChunk() {
  int size = (int)data_.size();
  const char* chunk_ptr = data_.c_str() + cur_pos_;
  int bytes_to_send = std::min(options_.chunk_size_, size - cur_pos_);

  socket_->Send(chunk_ptr, bytes_to_send);
  VLOG(1) << "Sent(" << cur_pos_ << "," << bytes_to_send << "): "
          << base::StringPiece(chunk_ptr, bytes_to_send);

  cur_pos_ += bytes_to_send;
  if (cur_pos_ < size) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&ConfigurableConnection::SendChunk, this),
        base::TimeDelta::FromMilliseconds(options_.timeout_));
  } else {
    socket_ = 0;  // close the connection.
  }
}

void ConfigurableConnection::Close() {
  socket_ = NULL;
}

void ConfigurableConnection::Send(const std::string& headers,
                                  const std::string& content) {
  SendOptions options(SendOptions::IMMEDIATE, 0, 0);
  SendWithOptions(headers, content, options);
}

void ConfigurableConnection::SendWithOptions(const std::string& headers,
                                             const std::string& content,
                                             const SendOptions& options) {
  std::string content_length_header;
  if (!content.empty() &&
      std::string::npos == headers.find("Context-Length:")) {
    content_length_header = base::StringPrintf("Content-Length: %u\r\n",
                                               content.size());
  }

  // Save the options.
  options_ = options;

  if (options_.speed_ == SendOptions::IMMEDIATE) {
    socket_->Send(headers);
    socket_->Send(content_length_header, true);
    socket_->Send(content);
    // Post a task to close the socket since StreamListenSocket doesn't like
    // instances to go away from within its callbacks.
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&ConfigurableConnection::Close, this));

    return;
  }

  if (options_.speed_ == SendOptions::IMMEDIATE_HEADERS_DELAYED_CONTENT) {
    socket_->Send(headers);
    socket_->Send(content_length_header, true);
    VLOG(1) << "Headers sent: " << headers << content_length_header;
    data_.append(content);
  }

  if (options_.speed_ == SendOptions::DELAYED) {
    data_ = headers;
    data_.append(content_length_header);
    data_.append("\r\n");
  }

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&ConfigurableConnection::SendChunk, this),
      base::TimeDelta::FromMilliseconds(options.timeout_));
}

}  // namespace test_server
