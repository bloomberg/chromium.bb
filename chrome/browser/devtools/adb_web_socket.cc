// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/adb_web_socket.h"

#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/server/web_socket.h"

using content::BrowserThread;
using net::WebSocket;

const int kBufferSize = 16 * 1024;

static const char kWebSocketUpgradeRequest[] = "GET %s HTTP/1.1\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";

AdbWebSocket::AdbWebSocket(
    scoped_refptr<AndroidDevice> device,
    const std::string& socket_name,
    const std::string& url,
    base::MessageLoop* adb_message_loop,
    Delegate* delegate)
    : device_(device),
      socket_name_(socket_name),
      url_(url),
      adb_message_loop_(adb_message_loop),
      delegate_(delegate) {
  adb_message_loop_->PostTask(
      FROM_HERE, base::Bind(&AdbWebSocket::ConnectOnHandlerThread, this));
}

void AdbWebSocket::Disconnect() {
  adb_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AdbWebSocket::DisconnectOnHandlerThread, this, false));
  adb_message_loop_ = NULL;
}

void AdbWebSocket::SendFrame(const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  adb_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AdbWebSocket::SendFrameOnHandlerThread, this, message));
}

void AdbWebSocket::SendFrameOnHandlerThread(const std::string& message) {
  int mask = base::RandInt(0, 0x7FFFFFFF);
  std::string encoded_frame = WebSocket::EncodeFrameHybi17(message, mask);
  request_buffer_ += encoded_frame;
  if (request_buffer_.length() == encoded_frame.length())
    SendPendingRequests(0);
}

AdbWebSocket::~AdbWebSocket() {}

void AdbWebSocket::ConnectOnHandlerThread() {
  device_->HttpUpgrade(
      socket_name_,
      base::StringPrintf(kWebSocketUpgradeRequest, url_.c_str()),
      base::Bind(&AdbWebSocket::ConnectedOnHandlerThread, this));
}

void AdbWebSocket::ConnectedOnHandlerThread(
  int result, net::StreamSocket* socket) {
  if (result != net::OK || socket == NULL) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbWebSocket::OnSocketClosed, this, true));
    return;
  }
  socket_.reset(socket);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&AdbWebSocket::OnSocketOpened, this));
  StartListeningOnHandlerThread();
}

void AdbWebSocket::StartListeningOnHandlerThread() {
  scoped_refptr<net::IOBuffer> response_buffer =
      new net::IOBuffer(kBufferSize);
  int result = socket_->Read(
      response_buffer.get(),
      kBufferSize,
      base::Bind(&AdbWebSocket::OnBytesRead, this, response_buffer));
  if (result != net::ERR_IO_PENDING)
    OnBytesRead(response_buffer, result);
}

void AdbWebSocket::OnBytesRead(
    scoped_refptr<net::IOBuffer> response_buffer, int result) {
  if (!socket_)
    return;

  if (result <= 0) {
    DisconnectOnHandlerThread(true);
    return;
  }

  std::string data = std::string(response_buffer->data(), result);
  response_buffer_ += data;

  int bytes_consumed;
  std::string output;
  WebSocket::ParseResult parse_result = WebSocket::DecodeFrameHybi17(
      response_buffer_, false, &bytes_consumed, &output);

  while (parse_result == WebSocket::FRAME_OK) {
    response_buffer_ = response_buffer_.substr(bytes_consumed);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbWebSocket::OnFrameRead, this, output));
    parse_result = WebSocket::DecodeFrameHybi17(
        response_buffer_, false, &bytes_consumed, &output);
  }

  if (parse_result == WebSocket::FRAME_ERROR ||
      parse_result == WebSocket::FRAME_CLOSE) {
    DisconnectOnHandlerThread(true);
    return;
  }

  result = socket_->Read(
      response_buffer.get(),
      kBufferSize,
      base::Bind(&AdbWebSocket::OnBytesRead, this, response_buffer));
  if (result != net::ERR_IO_PENDING)
    OnBytesRead(response_buffer, result);
}

void AdbWebSocket::SendPendingRequests(int result) {
  if (!socket_)
    return;
  if (result < 0) {
    DisconnectOnHandlerThread(true);
    return;
  }
  request_buffer_ = request_buffer_.substr(result);
  if (request_buffer_.empty())
    return;

  scoped_refptr<net::StringIOBuffer> buffer =
      new net::StringIOBuffer(request_buffer_);
  result = socket_->Write(buffer.get(), buffer->size(),
                          base::Bind(&AdbWebSocket::SendPendingRequests,
                                     this));
  if (result != net::ERR_IO_PENDING)
    SendPendingRequests(result);
}

void AdbWebSocket::DisconnectOnHandlerThread(bool closed_by_device) {
  if (!socket_)
    return;
  // Wipe out socket_ first since Disconnect can re-enter this method.
  scoped_ptr<net::StreamSocket> socket(socket_.release());
  socket->Disconnect();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&AdbWebSocket::OnSocketClosed, this, closed_by_device));
}

void AdbWebSocket::OnSocketOpened() {
  delegate_->OnSocketOpened();
}

void AdbWebSocket::OnFrameRead(const std::string& message) {
  delegate_->OnFrameRead(message);
}

void AdbWebSocket::OnSocketClosed(bool closed_by_device) {
  delegate_->OnSocketClosed(closed_by_device);
}
