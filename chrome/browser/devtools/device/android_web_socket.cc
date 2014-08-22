// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/server/web_socket.h"
#include "net/socket/stream_socket.h"

using content::BrowserThread;
using net::WebSocket;

namespace {

const int kBufferSize = 16 * 1024;

class WebSocketImpl {
 public:
  typedef AndroidDeviceManager::AndroidWebSocket::Delegate Delegate;

  WebSocketImpl(Delegate* delegate,
                scoped_ptr<net::StreamSocket> socket);
  void StartListening();
  void SendFrame(const std::string& message);

 private:
  void OnBytesRead(scoped_refptr<net::IOBuffer> response_buffer, int result);
  void SendPendingRequests(int result);
  void Disconnect();

  Delegate* delegate_;
  scoped_ptr<net::StreamSocket> socket_;
  std::string response_buffer_;
  std::string request_buffer_;
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(WebSocketImpl);
};

class DelegateWrapper
    : public AndroidDeviceManager::AndroidWebSocket::Delegate {
 public:
  DelegateWrapper(base::WeakPtr<Delegate> weak_delegate,
                  scoped_refptr<base::MessageLoopProxy> message_loop)
      : weak_delegate_(weak_delegate),
        message_loop_(message_loop) {
  }

  virtual ~DelegateWrapper() {}

  // AndroidWebSocket::Delegate implementation
  virtual void OnSocketOpened() OVERRIDE {
    message_loop_->PostTask(FROM_HERE,
        base::Bind(&Delegate::OnSocketOpened, weak_delegate_));
  }

  virtual void OnFrameRead(const std::string& message) OVERRIDE {
    message_loop_->PostTask(FROM_HERE,
        base::Bind(&Delegate::OnFrameRead, weak_delegate_, message));
  }

  virtual void OnSocketClosed() OVERRIDE {
    message_loop_->PostTask(FROM_HERE,
        base::Bind(&Delegate::OnSocketClosed, weak_delegate_));
  }

 private:
  base::WeakPtr<Delegate> weak_delegate_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
};

class AndroidWebSocketImpl
    : public AndroidDeviceManager::AndroidWebSocket,
      public AndroidDeviceManager::AndroidWebSocket::Delegate {
 public:
  typedef AndroidDeviceManager::Device Device;
  AndroidWebSocketImpl(
      scoped_refptr<base::MessageLoopProxy> device_message_loop,
      scoped_refptr<Device> device,
      const std::string& socket_name,
      const std::string& url,
      AndroidWebSocket::Delegate* delegate);

  virtual ~AndroidWebSocketImpl();

  // AndroidWebSocket implementation
  virtual void SendFrame(const std::string& message) OVERRIDE;

  // AndroidWebSocket::Delegate implementation
  virtual void OnSocketOpened() OVERRIDE;
  virtual void OnFrameRead(const std::string& message) OVERRIDE;
  virtual void OnSocketClosed() OVERRIDE;

 private:
  void Connected(int result, scoped_ptr<net::StreamSocket> socket);

  scoped_refptr<base::MessageLoopProxy> device_message_loop_;
  scoped_refptr<Device> device_;
  std::string socket_name_;
  std::string url_;
  WebSocketImpl* connection_;
  DelegateWrapper* delegate_wrapper_;
  AndroidWebSocket::Delegate* delegate_;
  base::WeakPtrFactory<AndroidWebSocketImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AndroidWebSocketImpl);
};

AndroidWebSocketImpl::AndroidWebSocketImpl(
    scoped_refptr<base::MessageLoopProxy> device_message_loop,
    scoped_refptr<Device> device,
    const std::string& socket_name,
    const std::string& url,
    AndroidWebSocket::Delegate* delegate)
    : device_message_loop_(device_message_loop),
      device_(device),
      socket_name_(socket_name),
      url_(url),
      delegate_(delegate),
      weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(delegate_);
  device_->HttpUpgrade(
      socket_name_, url_,
      base::Bind(&AndroidWebSocketImpl::Connected, weak_factory_.GetWeakPtr()));
}

void AndroidWebSocketImpl::SendFrame(const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  device_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketImpl::SendFrame,
                 base::Unretained(connection_), message));
}

void WebSocketImpl::SendFrame(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!socket_)
    return;
  int mask = base::RandInt(0, 0x7FFFFFFF);
  std::string encoded_frame = WebSocket::EncodeFrameHybi17(message, mask);
  request_buffer_ += encoded_frame;
  if (request_buffer_.length() == encoded_frame.length())
    SendPendingRequests(0);
}

AndroidWebSocketImpl::~AndroidWebSocketImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  device_message_loop_->DeleteSoon(FROM_HERE, connection_);
  device_message_loop_->DeleteSoon(FROM_HERE, delegate_wrapper_);
}

WebSocketImpl::WebSocketImpl(Delegate* delegate,
                             scoped_ptr<net::StreamSocket> socket)
                             : delegate_(delegate),
                               socket_(socket.Pass()) {
  thread_checker_.DetachFromThread();
}

void AndroidWebSocketImpl::Connected(int result,
                                     scoped_ptr<net::StreamSocket> socket) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (result != net::OK || socket == NULL) {
    OnSocketClosed();
    return;
  }
  delegate_wrapper_ = new DelegateWrapper(weak_factory_.GetWeakPtr(),
                                          base::MessageLoopProxy::current());
  connection_ = new WebSocketImpl(delegate_wrapper_, socket.Pass());
  device_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketImpl::StartListening,
                 base::Unretained(connection_)));
  OnSocketOpened();
}

void WebSocketImpl::StartListening() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(socket_);
  scoped_refptr<net::IOBuffer> response_buffer =
      new net::IOBuffer(kBufferSize);
  int result = socket_->Read(
      response_buffer.get(),
      kBufferSize,
      base::Bind(&WebSocketImpl::OnBytesRead,
                 base::Unretained(this), response_buffer));
  if (result != net::ERR_IO_PENDING)
    OnBytesRead(response_buffer, result);
}

void WebSocketImpl::OnBytesRead(scoped_refptr<net::IOBuffer> response_buffer,
                                int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (result <= 0) {
    Disconnect();
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
    delegate_->OnFrameRead(output);
    parse_result = WebSocket::DecodeFrameHybi17(
        response_buffer_, false, &bytes_consumed, &output);
  }

  if (parse_result == WebSocket::FRAME_ERROR ||
      parse_result == WebSocket::FRAME_CLOSE) {
    Disconnect();
    return;
  }

  result = socket_->Read(
      response_buffer.get(),
      kBufferSize,
      base::Bind(&WebSocketImpl::OnBytesRead,
                 base::Unretained(this), response_buffer));
  if (result != net::ERR_IO_PENDING)
    OnBytesRead(response_buffer, result);
}

void WebSocketImpl::SendPendingRequests(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (result < 0) {
    Disconnect();
    return;
  }
  request_buffer_ = request_buffer_.substr(result);
  if (request_buffer_.empty())
    return;

  scoped_refptr<net::StringIOBuffer> buffer =
      new net::StringIOBuffer(request_buffer_);
  result = socket_->Write(buffer.get(), buffer->size(),
                          base::Bind(&WebSocketImpl::SendPendingRequests,
                                     base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    SendPendingRequests(result);
}

void WebSocketImpl::Disconnect() {
  DCHECK(thread_checker_.CalledOnValidThread());
  socket_.reset();
  delegate_->OnSocketClosed();
}

void AndroidWebSocketImpl::OnSocketOpened() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delegate_->OnSocketOpened();
}

void AndroidWebSocketImpl::OnFrameRead(const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delegate_->OnFrameRead(message);
}

void AndroidWebSocketImpl::OnSocketClosed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delegate_->OnSocketClosed();
}

}  // namespace

AndroidDeviceManager::AndroidWebSocket*
AndroidDeviceManager::Device::CreateWebSocket(
    const std::string& socket,
    const std::string& url,
    AndroidDeviceManager::AndroidWebSocket::Delegate* delegate) {
  return new AndroidWebSocketImpl(
      device_message_loop_, this, socket, url, delegate);
}
