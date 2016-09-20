// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/server/web_socket_encoder.h"
#include "net/socket/stream_socket.h"

using content::BrowserThread;
using net::WebSocket;

namespace {

const int kBufferSize = 16 * 1024;

}  // namespace

class AndroidDeviceManager::AndroidWebSocket::WebSocketImpl {
 public:
  WebSocketImpl(
      scoped_refptr<base::SingleThreadTaskRunner> response_task_runner,
      base::WeakPtr<AndroidWebSocket> weak_socket,
      const std::string& extensions,
      const std::string& body_head,
      std::unique_ptr<net::StreamSocket> socket)
      : response_task_runner_(response_task_runner),
        weak_socket_(weak_socket),
        socket_(std::move(socket)),
        encoder_(net::WebSocketEncoder::CreateClient(extensions)),
        response_buffer_(body_head) {
    thread_checker_.DetachFromThread();
  }

  void StartListening() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(socket_);

    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));

    if (response_buffer_.size() > 0)
      ProcessResponseBuffer(buffer);
    else
      Read(buffer);
  }

  void SendFrame(const std::string& message) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!socket_)
      return;
    int mask = base::RandInt(0, 0x7FFFFFFF);
    std::string encoded_frame;
    encoder_->EncodeFrame(message, mask, &encoded_frame);
    request_buffer_ += encoded_frame;
    if (request_buffer_.length() == encoded_frame.length())
      SendPendingRequests(0);
  }

 private:
  void Read(scoped_refptr<net::IOBuffer> io_buffer) {
    int result = socket_->Read(
        io_buffer.get(),
        kBufferSize,
        base::Bind(&WebSocketImpl::OnBytesRead,
                   base::Unretained(this), io_buffer));
    if (result != net::ERR_IO_PENDING)
      OnBytesRead(io_buffer, result);
  }

  void OnBytesRead(scoped_refptr<net::IOBuffer> io_buffer, int result) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (result <= 0) {
      Disconnect();
      return;
    }
    response_buffer_.append(io_buffer->data(), result);

    ProcessResponseBuffer(io_buffer);
  }

  void ProcessResponseBuffer(scoped_refptr<net::IOBuffer> io_buffer) {
    int bytes_consumed;
    std::string output;
    WebSocket::ParseResult parse_result = encoder_->DecodeFrame(
        response_buffer_, &bytes_consumed, &output);

    while (parse_result == WebSocket::FRAME_OK) {
      response_buffer_ = response_buffer_.substr(bytes_consumed);
      response_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&AndroidWebSocket::OnFrameRead, weak_socket_, output));
      parse_result = encoder_->DecodeFrame(
          response_buffer_, &bytes_consumed, &output);
    }

    if (parse_result == WebSocket::FRAME_ERROR ||
        parse_result == WebSocket::FRAME_CLOSE) {
      Disconnect();
      return;
    }
    Read(io_buffer);
  }

  void SendPendingRequests(int result) {
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

  void Disconnect() {
    DCHECK(thread_checker_.CalledOnValidThread());
    socket_.reset();
    response_task_runner_->PostTask(
        FROM_HERE, base::Bind(&AndroidWebSocket::OnSocketClosed, weak_socket_));
  }

  scoped_refptr<base::SingleThreadTaskRunner> response_task_runner_;
  base::WeakPtr<AndroidWebSocket> weak_socket_;
  std::unique_ptr<net::StreamSocket> socket_;
  std::unique_ptr<net::WebSocketEncoder> encoder_;
  std::string response_buffer_;
  std::string request_buffer_;
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(WebSocketImpl);
};

AndroidDeviceManager::AndroidWebSocket::AndroidWebSocket(
    scoped_refptr<Device> device,
    const std::string& socket_name,
    const std::string& path,
    Delegate* delegate)
    : device_(device),
      socket_impl_(nullptr),
      delegate_(delegate),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(delegate_);
  DCHECK(device_);
  device_->HttpUpgrade(
      socket_name, path, net::WebSocketEncoder::kClientExtensions,
      base::Bind(&AndroidWebSocket::Connected, weak_factory_.GetWeakPtr()));
}

AndroidDeviceManager::AndroidWebSocket::~AndroidWebSocket() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (socket_impl_)
    device_->task_runner_->DeleteSoon(FROM_HERE, socket_impl_);
}

void AndroidDeviceManager::AndroidWebSocket::SendFrame(
    const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(socket_impl_);
  DCHECK(device_);
  device_->task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebSocketImpl::SendFrame,
                            base::Unretained(socket_impl_), message));
}

void AndroidDeviceManager::AndroidWebSocket::Connected(
    int result,
    const std::string& extensions,
    const std::string& body_head,
    std::unique_ptr<net::StreamSocket> socket) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (result != net::OK || !socket.get()) {
    OnSocketClosed();
    return;
  }
  socket_impl_ = new WebSocketImpl(base::ThreadTaskRunnerHandle::Get(),
                                   weak_factory_.GetWeakPtr(), extensions,
                                   body_head, std::move(socket));
  device_->task_runner_->PostTask(FROM_HERE,
                                  base::Bind(&WebSocketImpl::StartListening,
                                             base::Unretained(socket_impl_)));
  delegate_->OnSocketOpened();
}

void AndroidDeviceManager::AndroidWebSocket::OnFrameRead(
    const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnFrameRead(message);
}

void AndroidDeviceManager::AndroidWebSocket::OnSocketClosed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnSocketClosed();
}

AndroidDeviceManager::AndroidWebSocket*
AndroidDeviceManager::Device::CreateWebSocket(
    const std::string& socket_name,
    const std::string& path,
    AndroidWebSocket::Delegate* delegate) {
  return new AndroidWebSocket(this, socket_name, path, delegate);
}
