// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/websocket.h"

#include "base/base64.h"
#include "base/memory/scoped_vector.h"
#include "base/rand_util.h"
#include "base/sha1.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_job.h"

WebSocket::WebSocket(
    net::URLRequestContextGetter* context_getter,
    const GURL& url,
    WebSocketListener* listener)
    : context_getter_(context_getter),
      url_(url),
      listener_(listener),
      connected_(false) {
  net::WebSocketJob::EnsureInit();
  web_socket_ = new net::WebSocketJob(this);
}

WebSocket::~WebSocket() {
  CHECK(thread_checker_.CalledOnValidThread());
  web_socket_->Close();
  web_socket_->DetachDelegate();
}

void WebSocket::Connect(const net::CompletionCallback& callback) {
  CHECK(thread_checker_.CalledOnValidThread());
  CHECK_EQ(net::WebSocketJob::INITIALIZED, web_socket_->state());

  connect_callback_ = callback;

  scoped_refptr<net::SocketStream> socket = new net::SocketStream(
      url_, web_socket_);
  socket->set_context(context_getter_->GetURLRequestContext());

  web_socket_->InitSocketStream(socket);
  web_socket_->Connect();
}

bool WebSocket::Send(const std::string& message) {
  CHECK(thread_checker_.CalledOnValidThread());

  net::WebSocketFrameHeader header;
  header.final = true;
  header.reserved1 = false;
  header.reserved2 = false;
  header.reserved3 = false;
  header.opcode = net::WebSocketFrameHeader::kOpCodeText;
  header.masked = true;
  header.payload_length = message.length();
  int header_size = net::GetWebSocketFrameHeaderSize(header);
  net::WebSocketMaskingKey masking_key = net::GenerateWebSocketMaskingKey();
  std::string header_str;
  header_str.resize(header_size);
  CHECK_EQ(header_size, net::WriteWebSocketFrameHeader(
      header, &masking_key, &header_str[0], header_str.length()));

  std::string masked_message = message;
  net::MaskWebSocketFramePayload(
      masking_key, 0, &masked_message[0], masked_message.length());
  std::string data = header_str + masked_message;
  return web_socket_->SendData(data.c_str(), data.length());
}

void WebSocket::OnConnected(net::SocketStream* socket,
                            int max_pending_send_allowed) {
  CHECK(base::Base64Encode(base::RandBytesAsString(16), &sec_key_));
  std::string handshake = base::StringPrintf(
      "GET %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: %s\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "\r\n",
      url_.path().c_str(),
      url_.host().c_str(),
      sec_key_.c_str());
  if (!web_socket_->SendData(handshake.c_str(), handshake.length()))
    OnConnectFinished(net::ERR_FAILED);
}

void WebSocket::OnSentData(net::SocketStream* socket,
                           int amount_sent) {}

void WebSocket::OnReceivedData(net::SocketStream* socket,
                               const char* data, int len) {
  net::WebSocketJob::State state = web_socket_->state();
  if (!connect_callback_.is_null()) {
    // WebSocketJob guarantees the first OnReceivedData call contains all
    // the response headers.
    const char kMagicKey[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string websocket_accept;
    CHECK(base::Base64Encode(base::SHA1HashString(sec_key_ + kMagicKey),
                             &websocket_accept));
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(
            net::HttpUtil::AssembleRawHeaders(data, len)));
    if (headers->response_code() != 101 ||
        !headers->HasHeaderValue("Upgrade", "WebSocket") ||
        !headers->HasHeaderValue("Connection", "Upgrade") ||
        !headers->HasHeaderValue("Sec-WebSocket-Accept", websocket_accept)) {
      OnConnectFinished(net::ERR_FAILED);
      return;
    }
    OnConnectFinished(
        state == net::WebSocketJob::OPEN ? net::OK : net::ERR_FAILED);
  } else if (connected_) {
    ScopedVector<net::WebSocketFrameChunk> frame_chunks;
    CHECK(parser_.Decode(data, len, &frame_chunks));
    for (size_t i = 0; i < frame_chunks.size(); ++i) {
      scoped_refptr<net::IOBufferWithSize> buffer = frame_chunks[i]->data;
      if (buffer)
        next_message_ += std::string(buffer->data(), buffer->size());
      if (frame_chunks[i]->final_chunk) {
        listener_->OnMessageReceived(next_message_);
        next_message_.clear();
      }
    }
  }
}

void WebSocket::OnClose(net::SocketStream* socket) {
  if (!connect_callback_.is_null())
    OnConnectFinished(net::ERR_CONNECTION_CLOSED);
  else
    listener_->OnClose();
}

void WebSocket::OnConnectFinished(net::Error error) {
  if (error == net::OK)
    connected_ = true;
  net::CompletionCallback temp = connect_callback_;
  connect_callback_.Reset();
  temp.Run(error);
}
