// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "network/public/cpp/udp_socket_wrapper.h"

#include <assert.h>

#include "third_party/mojo/src/mojo/public/cpp/environment/logging.h"

namespace mojo {
namespace {

const uint32_t kDefaultReceiveQueueSlots = 32;

}  // namespace

UDPSocketWrapper::NegotiateCallbackHandler::NegotiateCallbackHandler(
    UDPSocketWrapper* delegate)
    : delegate_(delegate) {
}

UDPSocketWrapper::NegotiateCallbackHandler::~NegotiateCallbackHandler() {}

void UDPSocketWrapper::NegotiateCallbackHandler::Run(
    uint32_t actual_size) const {
  delegate_->OnNegotiateMaxPendingSendRequestsCompleted(actual_size);
}

UDPSocketWrapper::SendCallbackHandler::SendCallbackHandler(
    UDPSocketWrapper* delegate,
    const ErrorCallback& forward_callback)
    : delegate_(delegate),
      forward_callback_(forward_callback) {
}

UDPSocketWrapper::SendCallbackHandler::~SendCallbackHandler() {}

void UDPSocketWrapper::SendCallbackHandler::Run(NetworkErrorPtr result) const {
  delegate_->OnSendToCompleted(result.Pass(), forward_callback_);
}

UDPSocketWrapper::ReceiverBindingCallback::ReceiverBindingCallback(
    UDPSocketWrapper* delegate,
    const Callback<void(NetworkErrorPtr, NetAddressPtr)>& wrapper_callback)
    : delegate_(delegate), wrapper_callback_(wrapper_callback) {
}

UDPSocketWrapper::ReceiverBindingCallback::~ReceiverBindingCallback() {
}

void UDPSocketWrapper::ReceiverBindingCallback::Run(
    NetworkErrorPtr result,
    NetAddressPtr addr,
    InterfaceRequest<UDPSocketReceiver> request) const {
  delegate_->StartReceivingData(request.Pass());
  wrapper_callback_.Run(result.Pass(), addr.Pass());
}

UDPSocketWrapper::ReceivedData::ReceivedData() {}
UDPSocketWrapper::ReceivedData::~ReceivedData() {}

UDPSocketWrapper::SendRequest::SendRequest() {}
UDPSocketWrapper::SendRequest::~SendRequest() {}

UDPSocketWrapper::UDPSocketWrapper(UDPSocketPtr socket)
    : binding_(this),
      socket_(socket.Pass()),
      max_receive_queue_size_(kDefaultReceiveQueueSlots),
      max_pending_sends_(1),
      current_pending_sends_(0) {
  Initialize(0);
}

UDPSocketWrapper::UDPSocketWrapper(UDPSocketPtr socket,
                                   uint32_t receive_queue_slots,
                                   uint32_t requested_max_pending_sends)
    : binding_(this),
      socket_(socket.Pass()),
      max_receive_queue_size_(receive_queue_slots),
      max_pending_sends_(1),
      current_pending_sends_(0) {
  Initialize(requested_max_pending_sends);
}

UDPSocketWrapper::~UDPSocketWrapper() {
  while (!receive_queue_.empty()) {
    delete receive_queue_.front();
    receive_queue_.pop();
  }
  while (!send_requests_.empty()) {
    delete send_requests_.front();
    send_requests_.pop();
  }
}

void UDPSocketWrapper::AllowAddressReuse(const ErrorCallback& callback) {
  socket_->AllowAddressReuse(callback);
}

void UDPSocketWrapper::Bind(
    NetAddressPtr addr,
    const Callback<void(NetworkErrorPtr, NetAddressPtr)>& callback) {
  socket_->Bind(
      addr.Pass(),
      BindOrConnectCallback(static_cast<BindOrConnectCallback::Runnable*>(
          new ReceiverBindingCallback(this, callback))));
}

void UDPSocketWrapper::Connect(
    NetAddressPtr remote_addr,
    const Callback<void(NetworkErrorPtr, NetAddressPtr)>& callback) {
  socket_->Connect(
      remote_addr.Pass(),
      BindOrConnectCallback(static_cast<BindOrConnectCallback::Runnable*>(
          new ReceiverBindingCallback(this, callback))));
}

void UDPSocketWrapper::SetSendBufferSize(uint32_t size,
                                         const ErrorCallback& callback) {
  socket_->SetSendBufferSize(size, callback);
}

void UDPSocketWrapper::SetReceiveBufferSize(uint32_t size,
                                            const ErrorCallback& callback) {
  socket_->SetReceiveBufferSize(size, callback);
}

bool UDPSocketWrapper::ReceiveFrom(const ReceiveCallback& callback) {
  if (receive_queue_.empty()) {
    receive_requests_.push(callback);
    return false;
  }

  ReceivedData* data = receive_queue_.front();
  receive_queue_.pop();
  socket_->ReceiveMore(1);
  callback.Run(data->result.Pass(), data->src_addr.Pass(), data->data.Pass());
  delete data;
  return true;
}

void UDPSocketWrapper::SendTo(NetAddressPtr dest_addr,
                              Array<uint8_t> data,
                              const ErrorCallback& callback) {
  if (current_pending_sends_ >= max_pending_sends_) {
    SendRequest* request = new SendRequest();
    request->dest_addr = dest_addr.Pass();
    request->data = data.Pass();
    request->callback = callback;
    send_requests_.push(request);
    return;
  }

  MOJO_DCHECK(send_requests_.empty());
  current_pending_sends_++;
  socket_->SendTo(dest_addr.Pass(), data.Pass(),
                  ErrorCallback(static_cast<ErrorCallback::Runnable*>(
                      new SendCallbackHandler(this, callback))));
}

void UDPSocketWrapper::OnReceived(NetworkErrorPtr result,
                                  NetAddressPtr src_addr,
                                  Array<uint8_t> data) {
  if (!receive_requests_.empty()) {
    // The cache should be empty if there are user requests waiting for data.
    MOJO_DCHECK(receive_queue_.empty());

    socket_->ReceiveMore(1);

    ReceiveCallback callback = receive_requests_.front();
    receive_requests_.pop();

    callback.Run(result.Pass(), src_addr.Pass(), data.Pass());
    return;
  }

  MOJO_DCHECK(receive_queue_.size() < max_receive_queue_size_);
  ReceivedData* received_data = new ReceivedData();
  received_data->result = result.Pass();
  received_data->src_addr = src_addr.Pass();
  received_data->data = data.Pass();
  receive_queue_.push(received_data);
}

void UDPSocketWrapper::Initialize(uint32_t requested_max_pending_sends) {
  socket_->NegotiateMaxPendingSendRequests(
      requested_max_pending_sends,
      Callback<void(uint32_t)>(
          static_cast< Callback<void(uint32_t)>::Runnable*>(
              new NegotiateCallbackHandler(this))));
}

void UDPSocketWrapper::OnNegotiateMaxPendingSendRequestsCompleted(
    uint32_t actual_size) {
  MOJO_DCHECK(max_pending_sends_ == 1);

  if (actual_size == 0) {
    assert(false);
    return;
  }

  max_pending_sends_ = actual_size;

  while (ProcessNextSendRequest());
}

void UDPSocketWrapper::OnSendToCompleted(
    NetworkErrorPtr result,
    const ErrorCallback& forward_callback) {
  current_pending_sends_--;
  ProcessNextSendRequest();

  forward_callback.Run(result.Pass());
}

bool UDPSocketWrapper::ProcessNextSendRequest() {
  if (current_pending_sends_ >= max_pending_sends_ || send_requests_.empty())
    return false;

  SendRequest* request = send_requests_.front();
  send_requests_.pop();

  current_pending_sends_++;

  socket_->SendTo(
      request->dest_addr.Pass(), request->data.Pass(),
      ErrorCallback(static_cast<ErrorCallback::Runnable*>(
          new SendCallbackHandler(this, request->callback))));

  delete request;

  return true;
}

void UDPSocketWrapper::StartReceivingData(
    InterfaceRequest<UDPSocketReceiver> request) {
  binding_.Bind(request.Pass());
  socket_->ReceiveMore(max_receive_queue_size_);
}

}  // namespace mojo
