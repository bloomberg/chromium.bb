// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <map>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "net/server/http_listen_socket.h"
#include "net/server/http_server_request_info.h"

// must run in the IO thread
HttpListenSocket::HttpListenSocket(SOCKET s,
                                   HttpListenSocket::Delegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ListenSocket(s, this)),
      delegate_(delegate),
      is_web_socket_(false) {
}

// must run in the IO thread
HttpListenSocket::~HttpListenSocket() {
}

void HttpListenSocket::Accept() {
  SOCKET conn = ListenSocket::Accept(socket_);
  DCHECK_NE(conn, ListenSocket::kInvalidSocket);
  if (conn == ListenSocket::kInvalidSocket) {
    // TODO
  } else {
    scoped_refptr<HttpListenSocket> sock =
        new HttpListenSocket(conn, delegate_);
    // it's up to the delegate to AddRef if it wants to keep it around
    DidAccept(this, sock);
  }
}

HttpListenSocket* HttpListenSocket::Listen(
    const std::string& ip,
    int port,
    HttpListenSocket::Delegate* delegate) {
  SOCKET s = ListenSocket::Listen(ip, port);
  if (s == ListenSocket::kInvalidSocket) {
    // TODO (ibrar): error handling
  } else {
    HttpListenSocket *serv = new HttpListenSocket(s, delegate);
    serv->Listen();
    return serv;
  }
  return NULL;
}

std::string GetHeaderValue(
    HttpServerRequestInfo* request,
    const std::string& header_name) {
  HttpServerRequestInfo::HeadersMap::iterator it =
      request->headers.find(header_name);
  if (it != request->headers.end())
    return it->second;
  return "";
}

uint32 WebSocketKeyFingerprint(const std::string& str) {
  std::string result;
  const char* pChar = str.c_str();
  int length = str.length();
  int spaces = 0;
  for (int i = 0; i < length; ++i) {
    if (pChar[i] >= '0' && pChar[i] <= '9')
      result.append(&pChar[i], 1);
    else if (pChar[i] == ' ')
      spaces++;
  }
  if (spaces == 0)
    return 0;
  int64 number = 0;
  if (!StringToInt64(result, &number))
    return 0;
  return htonl(static_cast<uint32>(number / spaces));
}

void HttpListenSocket::AcceptWebSocket(HttpServerRequestInfo* request) {
  std::string key1 = GetHeaderValue(request, "Sec-WebSocket-Key1");
  std::string key2 = GetHeaderValue(request, "Sec-WebSocket-Key2");

  uint32 fp1 = WebSocketKeyFingerprint(key1);
  uint32 fp2 = WebSocketKeyFingerprint(key2);

  char data[16];
  memcpy(data, &fp1, 4);
  memcpy(data + 4, &fp2, 4);
  memcpy(data + 8, &request->data[0], 8);

  MD5Digest digest;
  MD5Sum(data, 16, &digest);

  std::string origin = GetHeaderValue(request, "Origin");
  std::string host = GetHeaderValue(request, "Host");
  std::string location = "ws://" + host + request->path;
  is_web_socket_ = true;
  Send(StringPrintf("HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
                    "Upgrade: WebSocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Origin: %s\r\n"
                    "Sec-WebSocket-Location: %s\r\n"
                    "\r\n",
                    origin.c_str(),
                    location.c_str()));
  Send(reinterpret_cast<char*>(digest.a), 16);
}

void HttpListenSocket::SendOverWebSocket(const std::string& data) {
  DCHECK(is_web_socket_);
  char message_start = 0;
  char message_end = -1;
  Send(&message_start, 1);
  Send(data);
  Send(&message_end, 1);
}

//
// HTTP Request Parser
// This HTTP request parser uses a simple state machine to quickly parse
// through the headers.  The parser is not 100% complete, as it is designed
// for use in this simple test driver.
//
// Known issues:
//   - does not handle whitespace on first HTTP line correctly.  Expects
//     a single space between the method/url and url/protocol.

// Input character types.
enum header_parse_inputs {
  INPUT_SPACE,
  INPUT_CR,
  INPUT_LF,
  INPUT_COLON,
  INPUT_00,
  INPUT_FF,
  INPUT_DEFAULT,
  MAX_INPUTS,
};

// Parser states.
enum header_parse_states {
  ST_METHOD,     // Receiving the method
  ST_URL,        // Receiving the URL
  ST_PROTO,      // Receiving the protocol
  ST_HEADER,     // Starting a Request Header
  ST_NAME,       // Receiving a request header name
  ST_SEPARATOR,  // Receiving the separator between header name and value
  ST_VALUE,      // Receiving a request header value
  ST_WS_READY,   // Ready to receive web socket frame
  ST_WS_FRAME,   // Receiving WebSocket frame
  ST_WS_CLOSE,   // Closing the connection WebSocket connection
  ST_DONE,       // Parsing is complete and successful
  ST_ERR,        // Parsing encountered invalid syntax.
  MAX_STATES
};

// State transition table
int parser_state[MAX_STATES][MAX_INPUTS] = {
/* METHOD    */ { ST_URL,       ST_ERR,      ST_ERR,      ST_ERR,       ST_ERR,      ST_ERR,      ST_METHOD },
/* URL       */ { ST_PROTO,     ST_ERR,      ST_ERR,      ST_URL,       ST_ERR,      ST_ERR,      ST_URL },
/* PROTOCOL  */ { ST_ERR,       ST_HEADER,   ST_NAME,     ST_ERR,       ST_ERR,      ST_ERR,      ST_PROTO },
/* HEADER    */ { ST_ERR,       ST_ERR,      ST_NAME,     ST_ERR,       ST_ERR,      ST_ERR,      ST_ERR },
/* NAME      */ { ST_SEPARATOR, ST_DONE,     ST_ERR,      ST_SEPARATOR, ST_ERR,      ST_ERR,      ST_NAME },
/* SEPARATOR */ { ST_SEPARATOR, ST_ERR,      ST_ERR,      ST_SEPARATOR, ST_ERR,      ST_ERR,      ST_VALUE },
/* VALUE     */ { ST_VALUE,     ST_HEADER,   ST_NAME,     ST_VALUE,     ST_ERR,      ST_ERR,      ST_VALUE },
/* WS_READY  */ { ST_ERR,       ST_ERR,      ST_ERR,      ST_ERR,       ST_WS_FRAME, ST_WS_CLOSE, ST_ERR},
/* WS_FRAME  */ { ST_WS_FRAME,  ST_WS_FRAME, ST_WS_FRAME, ST_WS_FRAME,  ST_ERR,      ST_WS_READY, ST_WS_FRAME },
/* WS_CLOSE  */ { ST_ERR,       ST_ERR,      ST_ERR,      ST_ERR,       ST_WS_CLOSE, ST_ERR,      ST_ERR },
/* DONE      */ { ST_DONE,      ST_DONE,     ST_DONE,     ST_DONE,      ST_DONE,     ST_DONE,     ST_DONE },
/* ERR       */ { ST_ERR,       ST_ERR,      ST_ERR,      ST_ERR,       ST_ERR,      ST_ERR,      ST_ERR }
};

// Convert an input character to the parser's input token.
int charToInput(char ch) {
  switch(ch) {
    case ' ':
      return INPUT_SPACE;
    case '\r':
      return INPUT_CR;
    case '\n':
      return INPUT_LF;
    case ':':
      return INPUT_COLON;
    case 0x0:
      return INPUT_00;
    case static_cast<char>(-1):
      return INPUT_FF;
  }
  return INPUT_DEFAULT;
}

HttpServerRequestInfo* HttpListenSocket::ParseHeaders() {
  int pos = 0;
  int data_len = recv_data_.length();
  int state = is_web_socket_ ? ST_WS_READY : ST_METHOD;
  scoped_ptr<HttpServerRequestInfo> info(new HttpServerRequestInfo());
  std::string buffer;
  std::string header_name;
  std::string header_value;
  while (pos < data_len) {
    char ch = recv_data_[pos++];
    int input = charToInput(ch);
    int next_state = parser_state[state][input];

    bool transition = (next_state != state);
    if (transition) {
      // Do any actions based on state transitions.
      switch (state) {
        case ST_METHOD:
          info->method = buffer;
          buffer.clear();
          break;
        case ST_URL:
          info->path = buffer;
          buffer.clear();
          break;
        case ST_PROTO:
          // TODO(mbelshe): Deal better with parsing protocol.
          DCHECK(buffer == "HTTP/1.1");
          buffer.clear();
          break;
        case ST_NAME:
          header_name = buffer;
          buffer.clear();
          break;
        case ST_VALUE:
          header_value = buffer;
          // TODO(mbelshe): Deal better with duplicate headers
          DCHECK(info->headers.find(header_name) == info->headers.end());
          info->headers[header_name] = header_value;
          buffer.clear();
          break;
        case ST_SEPARATOR:
          buffer.append(&ch, 1);
          break;
        case ST_WS_FRAME:
          recv_data_ = recv_data_.substr(pos);
          info->data = buffer;
          buffer.clear();
          return info.release();
          break;
      }
      state = next_state;
    } else {
      // Do any actions based on current state
      switch (state) {
        case ST_METHOD:
        case ST_URL:
        case ST_PROTO:
        case ST_VALUE:
        case ST_NAME:
        case ST_WS_FRAME:
          buffer.append(&ch, 1);
          break;
        case ST_DONE:
          recv_data_ = recv_data_.substr(pos);
          info->data = recv_data_;
          recv_data_.clear();
          return info.release();
        case ST_WS_CLOSE:
          is_web_socket_ = false;
          return NULL;
        case ST_ERR:
          return NULL;
      }
    }
  }
  // No more characters, but we haven't finished parsing yet.
  return NULL;
}

void HttpListenSocket::DidAccept(ListenSocket* server,
                                 ListenSocket* connection) {
  connection->AddRef();
}

void HttpListenSocket::DidRead(ListenSocket*,
                               const char* data,
                               int len) {
  recv_data_.append(data, len);
  while (recv_data_.length()) {
    scoped_ptr<HttpServerRequestInfo> request(ParseHeaders());
    if (!request.get())
      break;

    if (is_web_socket_) {
      delegate_->OnWebSocketMessage(this, request->data);
      continue;
    }

    std::string connection = GetHeaderValue(request.get(), "Connection");
    if (connection == "Upgrade") {
      // Is this WebSocket and if yes, upgrade the connection.
      std::string key1 = GetHeaderValue(request.get(), "Sec-WebSocket-Key1");
      std::string key2 = GetHeaderValue(request.get(), "Sec-WebSocket-Key2");
      if (!key1.empty() && !key2.empty()) {
        delegate_->OnWebSocketRequest(this, request.get());
        continue;
      }
    }
    delegate_->OnHttpRequest(this, request.get());
  }
}

void HttpListenSocket::DidClose(ListenSocket* sock) {
  sock->Release();
  delegate_->OnClose(this);
}
