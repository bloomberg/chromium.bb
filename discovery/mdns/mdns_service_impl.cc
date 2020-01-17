// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_service_impl.h"

#include "discovery/mdns/mdns_records.h"

namespace openscreen {
namespace discovery {
namespace {

static constexpr uint16_t kMdnsPortNumber = 5353;

}  // namespace

void MdnsServiceImpl::ReceiverClient::SetReceiver(MdnsReceiver* receiver) {
  OSP_DCHECK(!!receiver != !!receiver_);
  receiver_ = receiver;
}

void MdnsServiceImpl::ReceiverClient::OnError(UdpSocket* socket, Error error) {
  OSP_DCHECK(receiver_);
  receiver_->OnError(socket, std::move(error));
}

void MdnsServiceImpl::ReceiverClient::OnSendError(UdpSocket* socket,
                                                  Error error) {
  OSP_DCHECK(receiver_);
  receiver_->OnSendError(socket, std::move(error));
}

void MdnsServiceImpl::ReceiverClient::OnRead(UdpSocket* socket,
                                             ErrorOr<UdpPacket> packet) {
  OSP_DCHECK(receiver_);
  receiver_->OnRead(socket, std::move(packet));
}

// static
std::unique_ptr<MdnsService> MdnsService::Create(TaskRunner* task_runner) {
  IPEndpoint endpoint;
  endpoint.port = kMdnsPortNumber;
  auto responder = std::make_unique<MdnsServiceImpl::ReceiverClient>();
  ErrorOr<std::unique_ptr<UdpSocket>> socket =
      UdpSocket::Create(task_runner, responder.get(), std::move(endpoint));
  if (socket.is_error()) {
    return std::unique_ptr<MdnsService>();
  }

  return std::make_unique<MdnsServiceImpl>(
      task_runner, Clock::now, std::move(responder), std::move(socket.value()));
}

MdnsServiceImpl::MdnsServiceImpl(TaskRunner* task_runner,
                                 ClockNowFunctionPtr now_function,
                                 std::unique_ptr<ReceiverClient> callback,
                                 std::unique_ptr<UdpSocket> socket)
    : task_runner_(task_runner),
      now_function_(now_function),
      callback_(std::move(callback)),
      socket_(std::move(socket)),
      sender_(socket_.get()),
      receiver_(socket.get()),
      querier_(&sender_,
               &receiver_,
               task_runner_,
               now_function_,
               &random_delay_),
      probe_manager_(&sender_,
                     &receiver_,
                     &random_delay_,
                     task_runner_,
                     Clock::now),
      publisher_(&sender_, &probe_manager_, task_runner_, now_function_),
      responder_(&publisher_,
                 &probe_manager_,
                 &sender_,
                 &receiver_,
                 task_runner_,
                 &random_delay_) {
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(socket_.get());
  OSP_DCHECK(callback_.get());

  callback->SetReceiver(&receiver_);
}

void MdnsServiceImpl::StartQuery(const DomainName& name,
                                 DnsType dns_type,
                                 DnsClass dns_class,
                                 MdnsRecordChangedCallback* callback) {
  return querier_.StartQuery(name, dns_type, dns_class, callback);
}

void MdnsServiceImpl::StopQuery(const DomainName& name,
                                DnsType dns_type,
                                DnsClass dns_class,
                                MdnsRecordChangedCallback* callback) {
  return querier_.StopQuery(name, dns_type, dns_class, callback);
}

void MdnsServiceImpl::ReinitializeQueries(const DomainName& name) {
  querier_.ReinitializeQueries(name);
}

Error MdnsServiceImpl::StartProbe(MdnsDomainConfirmedProvider* callback,
                                  DomainName requested_name,
                                  IPAddress address) {
  return probe_manager_.StartProbe(callback, std::move(requested_name),
                                   std::move(address));
}

Error MdnsServiceImpl::RegisterRecord(const MdnsRecord& record) {
  return publisher_.RegisterRecord(record);
}

Error MdnsServiceImpl::UpdateRegisteredRecord(const MdnsRecord& old_record,
                                              const MdnsRecord& new_record) {
  return publisher_.UpdateRegisteredRecord(old_record, new_record);
}

Error MdnsServiceImpl::UnregisterRecord(const MdnsRecord& record) {
  return publisher_.UnregisterRecord(record);
}

}  // namespace discovery
}  // namespace openscreen
