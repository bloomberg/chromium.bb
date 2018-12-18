// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connectivity_probing_manager.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/log/net_log.h"

namespace net {

namespace {

// Default to 2 seconds timeout as the maximum timeout.
const int64_t kMaxProbingTimeoutMs = 2000;

std::unique_ptr<base::Value> NetLogStartProbingCallback(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress* peer_address,
    base::TimeDelta initial_timeout,
    NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetKey("network", NetLogNumberValue(network));
  dict->SetString("peer address", peer_address->ToString());
  dict->SetKey("initial_timeout_ms",
               NetLogNumberValue(initial_timeout.InMilliseconds()));
  return std::move(dict);
}

std::unique_ptr<base::Value> NetLogProbeReceivedCallback(
    NetworkChangeNotifier::NetworkHandle network,
    const IPEndPoint* self_address,
    const quic::QuicSocketAddress* peer_address,
    NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetKey("network", NetLogNumberValue(network));
  dict->SetString("self address", self_address->ToString());
  dict->SetString("peer address", peer_address->ToString());
  return std::move(dict);
}

std::unique_ptr<base::Value> NetLogProbingDestinationCallback(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress* peer_address,
    NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("network", base::Int64ToString(network));
  dict->SetString("peer address", peer_address->ToString());
  return std::move(dict);
}

}  // namespace

QuicConnectivityProbingManager::QuicConnectivityProbingManager(
    Delegate* delegate,
    base::SequencedTaskRunner* task_runner)
    : delegate_(delegate),
      is_running_(false),
      network_(NetworkChangeNotifier::kInvalidNetworkHandle),
      retry_count_(0),
      probe_start_time_(base::TimeTicks()),
      task_runner_(task_runner),
      weak_factory_(this) {
  retransmit_timer_.SetTaskRunner(task_runner_);
}

QuicConnectivityProbingManager::~QuicConnectivityProbingManager() {
  CancelProbingIfAny();
}

int QuicConnectivityProbingManager::HandleWriteError(
    int error_code,
    scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer> packet) {
  // Write error on the probing network is not recoverable.
  DVLOG(1) << "Probing packet encounters write error";
  // Post a task to notify |delegate_| that this probe failed and cancel
  // undergoing probing, which will delete the packet writer.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&QuicConnectivityProbingManager::NotifyDelegateProbeFailed,
                 weak_factory_.GetWeakPtr()));
  return error_code;
}

void QuicConnectivityProbingManager::OnWriteError(int error_code) {
  // Write error on the probing network.
  NotifyDelegateProbeFailed();
}

void QuicConnectivityProbingManager::OnWriteUnblocked() {}

void QuicConnectivityProbingManager::CancelProbing(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress& peer_address) {
  if (is_running_ && network == network_ && peer_address == peer_address_)
    CancelProbingIfAny();
}

void QuicConnectivityProbingManager::CancelProbingIfAny() {
  if (is_running_) {
    net_log_.AddEvent(
        NetLogEventType::QUIC_CONNECTIVITY_PROBING_MANAGER_CANCEL_PROBING,
        base::Bind(&NetLogProbingDestinationCallback, network_,
                   &peer_address_));
  }
  is_running_ = false;
  network_ = NetworkChangeNotifier::kInvalidNetworkHandle;
  peer_address_ = quic::QuicSocketAddress();
  socket_.reset();
  writer_.reset();
  reader_.reset();
  retry_count_ = 0;
  probe_start_time_ = base::TimeTicks();
  initial_timeout_ = base::TimeDelta();
  retransmit_timer_.Stop();
}

void QuicConnectivityProbingManager::StartProbing(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress& peer_address,
    std::unique_ptr<DatagramClientSocket> socket,
    std::unique_ptr<QuicChromiumPacketWriter> writer,
    std::unique_ptr<QuicChromiumPacketReader> reader,
    base::TimeDelta initial_timeout,
    const NetLogWithSource& net_log) {
  DCHECK(peer_address != quic::QuicSocketAddress());

  if (IsUnderProbing(network, peer_address))
    return;

  // Start a new probe will always cancel the previous one.
  CancelProbingIfAny();

  is_running_ = true;
  network_ = network;
  peer_address_ = peer_address;
  socket_ = std::move(socket);
  writer_ = std::move(writer);
  net_log_ = net_log;
  probe_start_time_ = base::TimeTicks::Now();

  // |this| will listen to all socket write events for the probing
  // packet writer.
  writer_->set_delegate(this);
  reader_ = std::move(reader);
  initial_timeout_ = initial_timeout;

  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTIVITY_PROBING_MANAGER_START_PROBING,
      base::Bind(&NetLogStartProbingCallback, network_, &peer_address_,
                 initial_timeout_));

  reader_->StartReading();
  SendConnectivityProbingPacket(initial_timeout_);
}

void QuicConnectivityProbingManager::OnConnectivityProbingReceived(
    const quic::QuicSocketAddress& self_address,
    const quic::QuicSocketAddress& peer_address) {
  if (!socket_) {
    DVLOG(1) << "Probing response is ignored as probing was cancelled "
             << "or succeeded.";
    return;
  }

  IPEndPoint local_address;
  socket_->GetLocalAddress(&local_address);

  DVLOG(1) << "Current probing is live at self ip:port "
           << local_address.ToString() << ", to peer ip:port "
           << peer_address_.ToString();

  if (quic::QuicSocketAddressImpl(local_address) != self_address.impl() ||
      peer_address_ != peer_address) {
    DVLOG(1) << "Received probing response from peer ip:port "
             << peer_address.ToString() << ", to self ip:port "
             << self_address.ToString() << ". Ignored.";
    return;
  }

  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTIVITY_PROBING_MANAGER_PROBE_RECEIVED,
      base::Bind(&NetLogProbeReceivedCallback, network_, &local_address,
                 &peer_address_));

  UMA_HISTOGRAM_COUNTS_100("Net.QuicSession.ProbingRetryCountUntilSuccess",
                           retry_count_);

  UMA_HISTOGRAM_TIMES("Net.QuicSession.ProbingTimeInMillisecondsUntilSuccess",
                      base::TimeTicks::Now() - probe_start_time_);

  // Notify the delegate that the probe succeeds and reset everything.
  delegate_->OnProbeSucceeded(network_, peer_address_, self_address,
                              std::move(socket_), std::move(writer_),
                              std::move(reader_));
  CancelProbingIfAny();
}

void QuicConnectivityProbingManager::SendConnectivityProbingPacket(
    base::TimeDelta timeout) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTIVITY_PROBING_MANAGER_PROBE_SENT,
      NetLog::Int64Callback("sent_count", retry_count_));
  if (!delegate_->OnSendConnectivityProbingPacket(writer_.get(),
                                                  peer_address_)) {
    NotifyDelegateProbeFailed();
    return;
  }
  retransmit_timer_.Start(
      FROM_HERE, timeout,
      base::Bind(
          &QuicConnectivityProbingManager::MaybeResendConnectivityProbingPacket,
          weak_factory_.GetWeakPtr()));
}

void QuicConnectivityProbingManager::NotifyDelegateProbeFailed() {
  if (is_running_) {
    delegate_->OnProbeFailed(network_, peer_address_);
    CancelProbingIfAny();
  }
}

void QuicConnectivityProbingManager::MaybeResendConnectivityProbingPacket() {
  // Use exponential backoff for the timeout.
  retry_count_++;
  int64_t timeout_ms =
      (UINT64_C(1) << retry_count_) * initial_timeout_.InMilliseconds();
  if (timeout_ms > kMaxProbingTimeoutMs) {
    NotifyDelegateProbeFailed();
    return;
  }
  SendConnectivityProbingPacket(base::TimeDelta::FromMilliseconds(timeout_ms));
}

}  // namespace net
