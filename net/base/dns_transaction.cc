// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dns_transaction.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "net/base/completion_callback.h"
#include "net/base/dns_query.h"
#include "net/base/dns_response.h"
#include "net/base/rand_callback.h"
#include "net/socket/client_socket_factory.h"
#include "net/udp/datagram_client_socket.h"

namespace net {

namespace {

// Retry timeouts.
const int kTimeoutsMs[] = {3000, 5000, 11000};
const int kMaxAttempts = arraysize(kTimeoutsMs);

}

DnsTransaction::Delegate::Delegate() {
}

DnsTransaction::Delegate::~Delegate() {
  while (!registered_transactions_.empty()) {
    DnsTransaction* transaction = *registered_transactions_.begin();
    transaction->SetDelegate(NULL);
  }
  DCHECK(registered_transactions_.empty());
}

void DnsTransaction::Delegate::OnTransactionComplete(
    int result,
    const DnsTransaction* transaction,
    const IPAddressList& ip_addresses) {
}

void DnsTransaction::Delegate::Attach(DnsTransaction* transaction) {
  DCHECK(registered_transactions_.find(transaction) ==
         registered_transactions_.end());
  registered_transactions_.insert(transaction);
}

void DnsTransaction::Delegate::Detach(DnsTransaction* transaction) {
  DCHECK(registered_transactions_.find(transaction) !=
         registered_transactions_.end());
  registered_transactions_.erase(transaction);
}

DnsTransaction::DnsTransaction(const IPEndPoint& dns_server,
                               const std::string& dns_name,
                               uint16 query_type,
                               const RandIntCallback& rand_int,
                               ClientSocketFactory* socket_factory)
    : dns_server_(dns_server),
      key_(dns_name, query_type),
      delegate_(NULL),
      query_(new DnsQuery(dns_name, query_type, rand_int)),
      attempts_(0),
      next_state_(STATE_NONE),
      socket_factory_(socket_factory ? socket_factory :
          ClientSocketFactory::GetDefaultFactory()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &DnsTransaction::OnIOComplete)) {
  DCHECK(!rand_int.is_null());
  for (size_t i = 0; i < arraysize(kTimeoutsMs); ++i)
    timeouts_ms_.push_back(base::TimeDelta::FromMilliseconds(kTimeoutsMs[i]));
}

DnsTransaction::~DnsTransaction() {
  SetDelegate(NULL);
}

void DnsTransaction::SetDelegate(Delegate* delegate) {
  if (delegate == delegate_)
    return;
  if (delegate_)
    delegate_->Detach(this);
  delegate_ = delegate;
  if (delegate_)
    delegate_->Attach(this);
}

int DnsTransaction::Start() {
  DCHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_CONNECT;
  return DoLoop(OK);
}

int DnsTransaction::DoLoop(int result) {
  DCHECK_NE(STATE_NONE, next_state_);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_CONNECT:
        rv = DoConnect();
        break;
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      case STATE_SEND_QUERY:
        rv = DoSendQuery();
        break;
      case STATE_SEND_QUERY_COMPLETE:
        rv = DoSendQueryComplete(rv);
        break;
      case STATE_READ_RESPONSE:
        rv = DoReadResponse();
        break;
      case STATE_READ_RESPONSE_COMPLETE:
        rv = DoReadResponseComplete(rv);
        break;
      default:
        NOTREACHED();
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

void DnsTransaction::DoCallback(int result) {
  DCHECK_NE(result, ERR_IO_PENDING);
  if (delegate_)
    delegate_->OnTransactionComplete(result, this, ip_addresses_);
}

void DnsTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int DnsTransaction::DoConnect() {
  next_state_ = STATE_CONNECT_COMPLETE;

  DCHECK_LT(attempts_, timeouts_ms_.size());
  StartTimer(timeouts_ms_[attempts_]);
  attempts_++;

  // TODO(agayev): keep all sockets around in case the server responds
  // after its timeout; state machine will need to change to handle that.
  socket_.reset(socket_factory_->CreateDatagramClientSocket(
      DatagramSocket::RANDOM_BIND,
      base::Bind(&base::RandInt),
      NULL,
      NetLog::Source()));
  return socket_->Connect(dns_server_);
}

int DnsTransaction::DoConnectComplete(int rv) {
  if (rv < 0)
    return rv;
  next_state_ = STATE_SEND_QUERY;
  return OK;
}

int DnsTransaction::DoSendQuery() {
  next_state_ = STATE_SEND_QUERY_COMPLETE;
  return socket_->Write(query_->io_buffer(),
                        query_->io_buffer()->size(),
                        &io_callback_);
}

int DnsTransaction::DoSendQueryComplete(int rv) {
  if (rv < 0)
    return rv;

  // Writing to UDP should not result in a partial datagram.
  if (rv != query_->io_buffer()->size())
    return ERR_NAME_NOT_RESOLVED;

  next_state_ = STATE_READ_RESPONSE;
  return OK;
}

int DnsTransaction::DoReadResponse() {
  next_state_ = STATE_READ_RESPONSE_COMPLETE;
  response_.reset(new DnsResponse(query_.get()));
  return socket_->Read(response_->io_buffer(),
                       response_->io_buffer()->size(),
                       &io_callback_);
}

int DnsTransaction::DoReadResponseComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  RevokeTimer();
  if (rv < 0)
    return rv;

  DCHECK(rv);
  // TODO(agayev): when supporting EDNS0 we may need to do multiple reads
  // to read the whole response.
  return response_->Parse(rv, &ip_addresses_);
}

void DnsTransaction::StartTimer(base::TimeDelta delay) {
  timer_.Start(delay, this, &DnsTransaction::OnTimeout);
}

void DnsTransaction::RevokeTimer() {
  timer_.Stop();
}

void DnsTransaction::OnTimeout() {
  DCHECK(next_state_ == STATE_SEND_QUERY_COMPLETE ||
         next_state_ == STATE_READ_RESPONSE_COMPLETE);
  if (attempts_ == timeouts_ms_.size()) {
    DoCallback(ERR_DNS_TIMED_OUT);
    return;
  }
  next_state_ = STATE_CONNECT;
  query_.reset(query_->CloneWithNewId());
  int rv = DoLoop(OK);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

void DnsTransaction::set_timeouts_ms(
    const std::vector<base::TimeDelta>& timeouts_ms) {
  DCHECK_EQ(0u, attempts_);
  timeouts_ms_ = timeouts_ms;
}

}  // namespace net
