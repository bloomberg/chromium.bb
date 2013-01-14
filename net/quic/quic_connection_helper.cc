// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_helper.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/congestion_control/quic_send_scheduler.h"
#include "net/quic/quic_utils.h"

namespace net {

// Limit the number of packets we send per resend-alarm so we eventually cede.
// 10 is arbitrary.
const size_t kMaxPacketsPerResendAlarm = 10;

QuicConnectionHelper::QuicConnectionHelper(base::TaskRunner* task_runner,
                                           const QuicClock* clock,
                                           QuicRandom* random_generator,
                                           DatagramClientSocket* socket)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      task_runner_(task_runner),
      socket_(socket),
      clock_(clock),
      random_generator_(random_generator),
      send_alarm_registered_(false),
      timeout_alarm_registered_(false),
      resend_alarm_registered_(false),
      resend_alarm_running_(false) {
}

QuicConnectionHelper::~QuicConnectionHelper() {
}

void QuicConnectionHelper::SetConnection(QuicConnection* connection) {
  connection_ = connection;
}

const QuicClock* QuicConnectionHelper::GetClock() const {
  return clock_;
}

QuicRandom* QuicConnectionHelper::GetRandomGenerator() {
  return random_generator_;
}

int QuicConnectionHelper::WritePacketToWire(
    const QuicEncryptedPacket& packet,
    int* error) {
  if (connection_->ShouldSimulateLostPacket()) {
    DLOG(INFO) << "Dropping packet due to fake packet loss.";
    *error = 0;
    return packet.length();
  }

  scoped_refptr<StringIOBuffer> buf(
      new StringIOBuffer(std::string(packet.data(),
                                     packet.length())));
  int rv = socket_->Write(buf, packet.length(),
                          base::Bind(&QuicConnectionHelper::OnWriteComplete,
                                     weak_factory_.GetWeakPtr()));
  if (rv >= 0) {
    *error = 0;
  } else {
    *error = rv;
    rv = -1;
  }
  return rv;
}

void QuicConnectionHelper::SetResendAlarm(
    QuicPacketSequenceNumber sequence_number,
    QuicTime::Delta delay) {
  if (!resend_alarm_registered_ &&
      !resend_alarm_running_) {
    resend_alarm_registered_ = true;
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&QuicConnectionHelper::OnResendAlarm,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(delay.ToMicroseconds()));
  }
  resend_times_[sequence_number] = clock_->Now().Add(delay);
}

void QuicConnectionHelper::SetSendAlarm(QuicTime::Delta delay) {
  send_alarm_registered_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&QuicConnectionHelper::OnSendAlarm,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMicroseconds(delay.ToMicroseconds()));
}

void QuicConnectionHelper::SetTimeoutAlarm(QuicTime::Delta delay) {
  DCHECK(!timeout_alarm_registered_);
  timeout_alarm_registered_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&QuicConnectionHelper::OnTimeoutAlarm,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMicroseconds(delay.ToMicroseconds()));
}

bool QuicConnectionHelper::IsSendAlarmSet() {
  return send_alarm_registered_;
}

void QuicConnectionHelper::UnregisterSendAlarmIfRegistered() {
  send_alarm_registered_ = false;
}

int QuicConnectionHelper::Read(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) {
  return socket_->Read(buf, buf_len, callback);
}

void QuicConnectionHelper::GetLocalAddress(IPEndPoint* local_address) {
  socket_->GetLocalAddress(local_address);
}

void QuicConnectionHelper::GetPeerAddress(IPEndPoint* peer_address) {
  socket_->GetPeerAddress(peer_address);
}

void QuicConnectionHelper::OnResendAlarm() {
  // This guards against registering the alarm later than we should.
  //
  // If we have packet A and B in the list and we call MaybeResendPacket on
  // A, that may trigger a call to SetResendAlarm if A is resent as C.  In
  // that case we don't want to register the alarm under SetResendAlarm; we
  // want to set it to the RTO of B at the end of this method.
  resend_alarm_registered_ = false;
  resend_alarm_running_ = true;

  for (size_t i = 0; i < kMaxPacketsPerResendAlarm; ++i) {
    if (resend_times_.empty() ||
        resend_times_.begin()->second > clock_->Now()) {
      break;
    }
    QuicPacketSequenceNumber sequence_number = resend_times_.begin()->first;
    connection_->MaybeResendPacket(sequence_number);
    resend_times_.erase(sequence_number);
  }

  // Nothing from here on does external calls.
  resend_alarm_running_ = false;
  if (resend_times_.empty()) {
    return;
  }

  // We have packet remaining.  Reschedule for the RTO of the oldest packet
  // on the list.
  SetResendAlarm(resend_times_.begin()->first,
                 resend_times_.begin()->second.Subtract(clock_->Now()));
}

void QuicConnectionHelper::OnSendAlarm() {
  if (send_alarm_registered_) {
    send_alarm_registered_ = false;
    connection_->OnCanWrite();
  }
}

void QuicConnectionHelper::OnTimeoutAlarm() {
  timeout_alarm_registered_ = false;
  connection_->CheckForTimeout();
}

void QuicConnectionHelper::OnWriteComplete(int result) {
  // TODO(rch): Inform the connection about the result.
  connection_->OnCanWrite();
}

}  // namespace net
