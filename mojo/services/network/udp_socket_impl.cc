// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/udp_socket_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <limits>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/net_address_type_converters.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/udp/datagram_socket.h"

namespace mojo {

namespace {

const int kMaxReadSize = 128 * 1024;
const size_t kMaxWriteSize = 128 * 1024;
const size_t kMaxPendingSendRequestsUpperbound = 128;
const size_t kDefaultMaxPendingSendRequests = 32;

}  // namespace

UDPSocketImpl::PendingSendRequest::PendingSendRequest() {}

UDPSocketImpl::PendingSendRequest::~PendingSendRequest() {}

UDPSocketImpl::UDPSocketImpl(InterfaceRequest<UDPSocket> request,
                             scoped_ptr<mojo::AppRefCount> app_refcount)
    : binding_(this, std::move(request)),
      socket_(net::DatagramSocket::DEFAULT_BIND,
              net::RandIntCallback(),
              nullptr,
              net::NetLog::Source()),
      state_(NOT_BOUND_OR_CONNECTED),
      allow_address_reuse_(false),
      remaining_recv_slots_(0),
      max_pending_send_requests_(kDefaultMaxPendingSendRequests),
      app_refcount_(std::move(app_refcount)) {}

UDPSocketImpl::~UDPSocketImpl() {
  STLDeleteElements(&pending_send_requests_);
}

void UDPSocketImpl::AllowAddressReuse(
    const Callback<void(NetworkErrorPtr)>& callback) {
  if (IsBoundOrConnected()) {
    callback.Run(MakeNetworkError(net::ERR_FAILED));
    return;
  }

  allow_address_reuse_ = true;
  callback.Run(MakeNetworkError(net::OK));
}

void UDPSocketImpl::Bind(
    NetAddressPtr addr,
    const Callback<void(NetworkErrorPtr,
                        NetAddressPtr,
                        InterfaceRequest<UDPSocketReceiver>)>& callback) {
  int net_result = net::OK;
  bool opened = false;

  do {
    if (IsBoundOrConnected()) {
      net_result = net::ERR_FAILED;
      break;
    }

    net::IPEndPoint ip_end_point = addr.To<net::IPEndPoint>();
    if (ip_end_point.GetFamily() == net::ADDRESS_FAMILY_UNSPECIFIED) {
      net_result = net::ERR_ADDRESS_INVALID;
      break;
    }

    net_result = socket_.Open(ip_end_point.GetFamily());
    if (net_result != net::OK)
      break;
    opened = true;

    if (allow_address_reuse_) {
      net_result = socket_.AllowAddressReuse();
      if (net_result != net::OK)
        break;
    }

    net_result = socket_.Bind(ip_end_point);
    if (net_result != net::OK)
      break;

    net::IPEndPoint bound_ip_end_point;
    net_result = socket_.GetLocalAddress(&bound_ip_end_point);
    if (net_result != net::OK)
      break;

    state_ = BOUND;
    callback.Run(MakeNetworkError(net_result),
                 NetAddress::From(bound_ip_end_point), GetProxy(&receiver_));

    if (remaining_recv_slots_ > 0) {
      DCHECK(!recvfrom_buffer_.get());
      DoRecvFrom();
    }
    return;
  } while (false);

  DCHECK(net_result != net::OK);
  if (opened)
    socket_.Close();
  callback.Run(MakeNetworkError(net_result), nullptr, nullptr);
}

void UDPSocketImpl::Connect(
    NetAddressPtr remote_addr,
    const Callback<void(NetworkErrorPtr,
                        NetAddressPtr,
                        InterfaceRequest<UDPSocketReceiver>)>& callback) {
  int net_result = net::OK;
  bool opened = false;

  do {
    if (IsBoundOrConnected()) {
      net_result = net::ERR_FAILED;
      break;
    }

    net::IPEndPoint ip_end_point = remote_addr.To<net::IPEndPoint>();
    if (ip_end_point.GetFamily() == net::ADDRESS_FAMILY_UNSPECIFIED) {
      net_result = net::ERR_ADDRESS_INVALID;
      break;
    }

    net_result = socket_.Open(ip_end_point.GetFamily());
    if (net_result != net::OK)
      break;
    opened = true;

    net_result = socket_.Connect(ip_end_point);
    if (net_result != net::OK)
      break;

    net::IPEndPoint local_ip_end_point;
    net_result = socket_.GetLocalAddress(&local_ip_end_point);
    if (net_result != net::OK)
      break;

    state_ = CONNECTED;
    callback.Run(MakeNetworkError(net_result),
                 NetAddress::From(local_ip_end_point), GetProxy(&receiver_));

    if (remaining_recv_slots_ > 0) {
      DCHECK(!recvfrom_buffer_.get());
      DoRecvFrom();
    }
    return;
  } while (false);

  DCHECK(net_result != net::OK);
  if (opened)
    socket_.Close();
  callback.Run(MakeNetworkError(net_result), nullptr, nullptr);
}

void UDPSocketImpl::SetSendBufferSize(
    uint32_t size,
    const Callback<void(NetworkErrorPtr)>& callback) {
  if (!IsBoundOrConnected()) {
    callback.Run(MakeNetworkError(net::ERR_FAILED));
    return;
  }

  if (size > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    size = std::numeric_limits<int32_t>::max();

  int net_result = socket_.SetSendBufferSize(static_cast<int32_t>(size));
  callback.Run(MakeNetworkError(net_result));
}

void UDPSocketImpl::SetReceiveBufferSize(
    uint32_t size,
    const Callback<void(NetworkErrorPtr)>& callback) {
  if (!IsBoundOrConnected()) {
    callback.Run(MakeNetworkError(net::ERR_FAILED));
    return;
  }

  if (size > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    size = std::numeric_limits<int32_t>::max();

  int net_result = socket_.SetReceiveBufferSize(static_cast<int32_t>(size));
  callback.Run(MakeNetworkError(net_result));
}

void UDPSocketImpl::NegotiateMaxPendingSendRequests(
    uint32_t requested_size,
    const Callback<void(uint32_t)>& callback) {
  if (requested_size != 0) {
    max_pending_send_requests_ =
      std::min(kMaxPendingSendRequestsUpperbound,
               static_cast<size_t>(requested_size));
  }
  callback.Run(static_cast<uint32_t>(max_pending_send_requests_));

  if (pending_send_requests_.size() > max_pending_send_requests_) {
    std::deque<PendingSendRequest*> discarded_requests(
        pending_send_requests_.begin() + max_pending_send_requests_,
        pending_send_requests_.end());
    pending_send_requests_.resize(max_pending_send_requests_);
    for (auto& discarded_request : discarded_requests) {
      discarded_request->callback.Run(
          MakeNetworkError(net::ERR_INSUFFICIENT_RESOURCES));
      delete discarded_request;
    }
  }
}

void UDPSocketImpl::ReceiveMore(uint32_t datagram_number) {
  if (!receiver_)
    return;
  if (datagram_number == 0)
    return;
  if (std::numeric_limits<size_t>::max() - remaining_recv_slots_ <
          datagram_number) {
    return;
  }

  remaining_recv_slots_ += datagram_number;

  if (IsBoundOrConnected() && !recvfrom_buffer_.get()) {
    DCHECK_EQ(datagram_number, remaining_recv_slots_);
    DoRecvFrom();
  }
}

void UDPSocketImpl::SendTo(NetAddressPtr dest_addr,
                           Array<uint8_t> data,
                           const Callback<void(NetworkErrorPtr)>& callback) {
  if (!IsBoundOrConnected()) {
    callback.Run(MakeNetworkError(net::ERR_FAILED));
    return;
  }
  if (state_ == BOUND && !dest_addr) {
    callback.Run(MakeNetworkError(net::ERR_INVALID_ARGUMENT));
    return;
  }

  if (sendto_buffer_.get()) {
    if (pending_send_requests_.size() >= max_pending_send_requests_) {
      callback.Run(MakeNetworkError(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }

    PendingSendRequest* request = new PendingSendRequest;
    request->addr = std::move(dest_addr);
    request->data = std::move(data);
    request->callback = callback;
    pending_send_requests_.push_back(request);
    return;
  }

  DCHECK_EQ(0u, pending_send_requests_.size());

  DoSendTo(std::move(dest_addr), std::move(data), callback);
}

void UDPSocketImpl::DoRecvFrom() {
  DCHECK(IsBoundOrConnected());
  DCHECK(receiver_);
  DCHECK(!recvfrom_buffer_.get());
  DCHECK_GT(remaining_recv_slots_, 0u);

  recvfrom_buffer_ = new net::IOBuffer(kMaxReadSize);

  // It is safe to use base::Unretained(this) because |socket_| is owned by this
  // object. If this object gets destroyed (and so does |socket_|), the callback
  // won't be called.
  int net_result = socket_.RecvFrom(
      recvfrom_buffer_.get(),
      kMaxReadSize,
      state_ == BOUND ? &recvfrom_address_ : nullptr,
      base::Bind(&UDPSocketImpl::OnRecvFromCompleted, base::Unretained(this)));
  if (net_result != net::ERR_IO_PENDING)
    OnRecvFromCompleted(net_result);
}

void UDPSocketImpl::DoSendTo(NetAddressPtr addr,
                             Array<uint8_t> data,
                             const Callback<void(NetworkErrorPtr)>& callback) {
  DCHECK(IsBoundOrConnected());
  DCHECK(!sendto_buffer_.get());

  if (data.size() > kMaxWriteSize) {
    callback.Run(MakeNetworkError(net::ERR_INVALID_ARGUMENT));
    return;
  }
  sendto_buffer_ = new net::IOBufferWithSize(static_cast<int>(data.size()));
  if (data.size() > 0)
    memcpy(sendto_buffer_->data(), &data.storage()[0], data.size());

  int net_result = net::OK;
  if (addr) {
    net::IPEndPoint ip_end_point = addr.To<net::IPEndPoint>();
    if (ip_end_point.GetFamily() == net::ADDRESS_FAMILY_UNSPECIFIED) {
      callback.Run(MakeNetworkError(net::ERR_ADDRESS_INVALID));
      return;
    }

    // It is safe to use base::Unretained(this) because |socket_| is owned by
    // this object. If this object gets destroyed (and so does |socket_|), the
    // callback won't be called.
    net_result = socket_.SendTo(sendto_buffer_.get(), sendto_buffer_->size(),
                                ip_end_point,
                                base::Bind(&UDPSocketImpl::OnSendToCompleted,
                                           base::Unretained(this), callback));
  } else {
    DCHECK(state_ == CONNECTED);
    net_result = socket_.Write(sendto_buffer_.get(), sendto_buffer_->size(),
                               base::Bind(&UDPSocketImpl::OnSendToCompleted,
                                          base::Unretained(this), callback));
  }
  if (net_result != net::ERR_IO_PENDING)
    OnSendToCompleted(callback, net_result);
}

void UDPSocketImpl::OnRecvFromCompleted(int net_result) {
  DCHECK(recvfrom_buffer_.get());

  NetAddressPtr net_address;
  Array<uint8_t> array;
  if (net_result >= 0) {
    if (state_ == BOUND)
      net_address = NetAddress::From(recvfrom_address_);

    std::vector<uint8_t> data(net_result);
    if (net_result > 0)
      memcpy(&data[0], recvfrom_buffer_->data(), net_result);

    array.Swap(&data);
  }
  recvfrom_buffer_ = nullptr;

  receiver_->OnReceived(MakeNetworkError(net_result), std::move(net_address),
                        std::move(array));
  DCHECK_GT(remaining_recv_slots_, 0u);
  remaining_recv_slots_--;
  if (remaining_recv_slots_ > 0)
    DoRecvFrom();
}

void UDPSocketImpl::OnSendToCompleted(
    const Callback<void(NetworkErrorPtr)>& callback,
    int net_result) {
  DCHECK(sendto_buffer_.get());

  sendto_buffer_ = nullptr;

  callback.Run(MakeNetworkError(net_result));

  if (pending_send_requests_.empty())
    return;

  scoped_ptr<PendingSendRequest> request(pending_send_requests_.front());
  pending_send_requests_.pop_front();

  DoSendTo(std::move(request->addr), std::move(request->data),
           request->callback);
}

}  // namespace mojo
