// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/android_usb_socket.h"

#include "base/message_loop/message_loop.h"

namespace {

const int kMaxPayload = 4096;

}  // namespace

AndroidUsbSocket::IORequest::IORequest(
    net::IOBuffer* buffer,
    int length,
    const net::CompletionCallback& callback)
    : buffer(buffer),
      length(length),
      callback(callback) {
}

AndroidUsbSocket::IORequest::~IORequest() {
}

AndroidUsbSocket::AndroidUsbSocket(scoped_refptr<AndroidUsbDevice> device,
                                   uint32 socket_id,
                                   const std::string& command,
                                   base::Callback<void(uint32)> delete_callback)
    : device_(device),
      command_(command),
      delete_callback_(delete_callback),
      local_id_(socket_id),
      remote_id_(0),
      is_connected_(false) {
}

AndroidUsbSocket::~AndroidUsbSocket() {
  DCHECK(CalledOnValidThread());
  if (is_connected_)
    Disconnect();
  if (!delete_callback_.is_null())
    delete_callback_.Run(local_id_);
}

void AndroidUsbSocket::HandleIncoming(scoped_refptr<AdbMessage> message) {
  if (!device_)
    return;

  CHECK_EQ(message->arg1, local_id_);
  switch (message->command) {
    case AdbMessage::kCommandOKAY:
      if (!is_connected_) {
        remote_id_ = message->arg0;
        is_connected_ = true;
        net::CompletionCallback callback = connect_callback_;
        connect_callback_.Reset();
        callback.Run(net::OK);
        // "this" can be NULL.
      } else {
        RespondToWriters();
        // "this" can be NULL.
      }
      break;
    case AdbMessage::kCommandWRTE:
      device_->Send(AdbMessage::kCommandOKAY, local_id_, message->arg0, "");
      read_buffer_ += message->body;
      // Allow WRTE over new connection even though OKAY ack was not received.
      if (!is_connected_) {
        remote_id_ = message->arg0;
        is_connected_ = true;
        net::CompletionCallback callback = connect_callback_;
        connect_callback_.Reset();
        callback.Run(net::OK);
        // "this" can be NULL.
      } else {
        RespondToReaders(false);
        // "this" can be NULL.
      }
      break;
    case AdbMessage::kCommandCLSE:
      if (is_connected_)
        device_->Send(AdbMessage::kCommandCLSE, local_id_, 0, "");
      Terminated(true);
      // "this" can be NULL.
      break;
    default:
      break;
  }
}

void AndroidUsbSocket::Terminated(bool closed_by_device) {
  is_connected_ = false;

  // Break the socket -> device connection, release the device.
  delete_callback_.Run(local_id_);
  delete_callback_.Reset();
  device_ = NULL;

  if (!closed_by_device)
    return;

  // Respond to pending callbacks.
  if (!connect_callback_.is_null()) {
    net::CompletionCallback callback = connect_callback_;
    connect_callback_.Reset();
    callback.Run(net::ERR_FAILED);
    // "this" can be NULL.
    return;
  }
  RespondToReaders(true);
}

int AndroidUsbSocket::Read(net::IOBuffer* buffer,
                           int length,
                           const net::CompletionCallback& callback) {
  if (!is_connected_)
    return device_ ? net::ERR_SOCKET_NOT_CONNECTED : 0;

  if (read_buffer_.empty()) {
    read_requests_.push_back(IORequest(buffer, length, callback));
    return net::ERR_IO_PENDING;
  }

  size_t bytes_to_copy = static_cast<size_t>(length) > read_buffer_.length() ?
      read_buffer_.length() : static_cast<size_t>(length);
  memcpy(buffer->data(), read_buffer_.data(), bytes_to_copy);
  if (read_buffer_.length() > bytes_to_copy)
    read_buffer_ = read_buffer_.substr(bytes_to_copy);
  else
    read_buffer_ = "";
  return bytes_to_copy;
}

int AndroidUsbSocket::Write(net::IOBuffer* buffer,
                            int length,
                            const net::CompletionCallback& callback) {
  if (!is_connected_)
    return net::ERR_SOCKET_NOT_CONNECTED;

  if (length > kMaxPayload)
    length = kMaxPayload;
  write_requests_.push_back(IORequest(NULL, length, callback));
  device_->Send(AdbMessage::kCommandWRTE, local_id_, remote_id_,
                 std::string(buffer->data(), length));
  return net::ERR_IO_PENDING;
}

int AndroidUsbSocket::SetReceiveBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int AndroidUsbSocket::SetSendBufferSize(int32 size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int AndroidUsbSocket::Connect(const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!device_)
    return net::ERR_FAILED;
  connect_callback_ = callback;
  device_->Send(AdbMessage::kCommandOPEN, local_id_, 0, command_);
  return net::ERR_IO_PENDING;
}

void AndroidUsbSocket::Disconnect() {
  if (!device_)
    return;
  device_->Send(AdbMessage::kCommandCLSE, local_id_, remote_id_, "");
  Terminated(false);
}

bool AndroidUsbSocket::IsConnected() const {
  DCHECK(CalledOnValidThread());
  return is_connected_;
}

bool AndroidUsbSocket::IsConnectedAndIdle() const {
  NOTIMPLEMENTED();
  return false;
}

int AndroidUsbSocket::GetPeerAddress(net::IPEndPoint* address) const {
  net::IPAddressNumber ip(net::kIPv4AddressSize);
  *address = net::IPEndPoint(ip, 0);
  return net::OK;
}

int AndroidUsbSocket::GetLocalAddress(net::IPEndPoint* address) const {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

const net::BoundNetLog& AndroidUsbSocket::NetLog() const {
  return net_log_;
}

void AndroidUsbSocket::SetSubresourceSpeculation() {
  NOTIMPLEMENTED();
}

void AndroidUsbSocket::SetOmniboxSpeculation() {
  NOTIMPLEMENTED();
}

bool AndroidUsbSocket::WasEverUsed() const {
  NOTIMPLEMENTED();
  return true;
}

bool AndroidUsbSocket::UsingTCPFastOpen() const {
  NOTIMPLEMENTED();
  return true;
}

bool AndroidUsbSocket::WasNpnNegotiated() const {
  NOTIMPLEMENTED();
  return true;
}

net::NextProto AndroidUsbSocket::GetNegotiatedProtocol() const {
  NOTIMPLEMENTED();
  return net::kProtoUnknown;
}

bool AndroidUsbSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  return false;
}

void AndroidUsbSocket::RespondToReaders(bool disconnect) {
  std::deque<IORequest> read_requests;
  read_requests.swap(read_requests_);
  while (!read_requests.empty() && (!read_buffer_.empty() || disconnect)) {
    IORequest read_request = read_requests.front();
    read_requests.pop_front();
    size_t bytes_to_copy =
        static_cast<size_t>(read_request.length) > read_buffer_.length() ?
            read_buffer_.length() : static_cast<size_t>(read_request.length);
    memcpy(read_request.buffer->data(), read_buffer_.data(), bytes_to_copy);
    if (read_buffer_.length() > bytes_to_copy)
      read_buffer_ = read_buffer_.substr(bytes_to_copy);
    else
      read_buffer_ = "";
    read_request.callback.Run(bytes_to_copy);
  }
}

void AndroidUsbSocket::RespondToWriters() {
  if (!write_requests_.empty()) {
    IORequest write_request = write_requests_.front();
    write_requests_.pop_front();
    write_request.callback.Run(write_request.length);
  }
}
