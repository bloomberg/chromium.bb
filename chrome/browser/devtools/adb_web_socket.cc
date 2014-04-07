// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/devtools_adb_bridge.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/server/web_socket.h"

using content::BrowserThread;
using net::WebSocket;

namespace {

const int kBufferSize = 16 * 1024;

static const char kWebSocketUpgradeRequest[] = "GET %s HTTP/1.1\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";

class AdbWebSocketImpl : public DevToolsAdbBridge::AdbWebSocket {
 public:
  AdbWebSocketImpl(scoped_refptr<DevToolsAdbBridge> adb_bridge,
                   AndroidDeviceManager* device_manager,
                   base::MessageLoop* device_message_loop,
                   const std::string& serial,
                   const std::string& socket_name,
                   const std::string& url,
                   Delegate* delegate);

  virtual void Disconnect() OVERRIDE;

  virtual void SendFrame(const std::string& message) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<AdbWebSocket>;

  virtual ~AdbWebSocketImpl();

  void ConnectOnHandlerThread();
  void ConnectedOnHandlerThread(int result, net::StreamSocket* socket);
  void StartListeningOnHandlerThread();
  void OnBytesRead(scoped_refptr<net::IOBuffer> response_buffer, int result);
  void SendFrameOnHandlerThread(const std::string& message);
  void SendPendingRequests(int result);
  void DisconnectOnHandlerThread(bool closed_by_device);

  void OnSocketOpened();
  void OnFrameRead(const std::string& message);
  void OnSocketClosed(bool closed_by_device);

  scoped_refptr<DevToolsAdbBridge> adb_bridge_;
  AndroidDeviceManager* device_manager_;
  base::MessageLoop* device_message_loop_;
  std::string serial_;
  std::string socket_name_;
  std::string url_;
  scoped_ptr<net::StreamSocket> socket_;
  Delegate* delegate_;
  std::string response_buffer_;
  std::string request_buffer_;
};

AdbWebSocketImpl::AdbWebSocketImpl(
    scoped_refptr<DevToolsAdbBridge> adb_bridge,
    AndroidDeviceManager* device_manager,
    base::MessageLoop* device_message_loop,
    const std::string& serial,
    const std::string& socket_name,
    const std::string& url,
    Delegate* delegate)
    : adb_bridge_(adb_bridge),
      device_manager_(device_manager),
      device_message_loop_(device_message_loop),
      serial_(serial),
      socket_name_(socket_name),
      url_(url),
      delegate_(delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  device_message_loop_->PostTask(
      FROM_HERE, base::Bind(&AdbWebSocketImpl::ConnectOnHandlerThread, this));
}

void AdbWebSocketImpl::Disconnect() {
  device_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AdbWebSocketImpl::DisconnectOnHandlerThread, this, false));
}

void AdbWebSocketImpl::SendFrame(const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  device_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AdbWebSocketImpl::SendFrameOnHandlerThread, this, message));
}

void AdbWebSocketImpl::SendFrameOnHandlerThread(const std::string& message) {
  int mask = base::RandInt(0, 0x7FFFFFFF);
  std::string encoded_frame = WebSocket::EncodeFrameHybi17(message, mask);
  request_buffer_ += encoded_frame;
  if (request_buffer_.length() == encoded_frame.length())
    SendPendingRequests(0);
}

AdbWebSocketImpl::~AdbWebSocketImpl() {}

void AdbWebSocketImpl::ConnectOnHandlerThread() {
  device_manager_->HttpUpgrade(
      serial_,
      socket_name_,
      base::StringPrintf(kWebSocketUpgradeRequest, url_.c_str()),
      base::Bind(&AdbWebSocketImpl::ConnectedOnHandlerThread, this));
}

void AdbWebSocketImpl::ConnectedOnHandlerThread(
  int result, net::StreamSocket* socket) {
  if (result != net::OK || socket == NULL) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbWebSocketImpl::OnSocketClosed, this, true));
    return;
  }
  socket_.reset(socket);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&AdbWebSocketImpl::OnSocketOpened, this));
  StartListeningOnHandlerThread();
}

void AdbWebSocketImpl::StartListeningOnHandlerThread() {
  scoped_refptr<net::IOBuffer> response_buffer =
      new net::IOBuffer(kBufferSize);
  int result = socket_->Read(
      response_buffer.get(),
      kBufferSize,
      base::Bind(&AdbWebSocketImpl::OnBytesRead, this, response_buffer));
  if (result != net::ERR_IO_PENDING)
    OnBytesRead(response_buffer, result);
}

void AdbWebSocketImpl::OnBytesRead(
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
        base::Bind(&AdbWebSocketImpl::OnFrameRead, this, output));
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
      base::Bind(&AdbWebSocketImpl::OnBytesRead, this, response_buffer));
  if (result != net::ERR_IO_PENDING)
    OnBytesRead(response_buffer, result);
}

void AdbWebSocketImpl::SendPendingRequests(int result) {
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
                          base::Bind(&AdbWebSocketImpl::SendPendingRequests,
                                     this));
  if (result != net::ERR_IO_PENDING)
    SendPendingRequests(result);
}

void AdbWebSocketImpl::DisconnectOnHandlerThread(bool closed_by_device) {
  if (!socket_)
    return;
  // Wipe out socket_ first since Disconnect can re-enter this method.
  scoped_ptr<net::StreamSocket> socket(socket_.release());
  socket->Disconnect();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&AdbWebSocketImpl::OnSocketClosed, this, closed_by_device));
}

void AdbWebSocketImpl::OnSocketOpened() {
  delegate_->OnSocketOpened();
}

void AdbWebSocketImpl::OnFrameRead(const std::string& message) {
  delegate_->OnFrameRead(message);
}

void AdbWebSocketImpl::OnSocketClosed(bool closed_by_device) {
  delegate_->OnSocketClosed(closed_by_device);
}

}  // namespace

scoped_refptr<DevToolsAdbBridge::AdbWebSocket>
DevToolsAdbBridge::RemoteBrowser::CreateWebSocket(
    const std::string& url,
    DevToolsAdbBridge::AdbWebSocket::Delegate* delegate) {
  return new AdbWebSocketImpl(
      adb_bridge_,
      adb_bridge_->device_manager(),
      adb_bridge_->device_message_loop(),
      serial_, socket_, url, delegate);
}
