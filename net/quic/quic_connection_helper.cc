// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_helper.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_utils.h"

namespace net {

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
      retransmission_alarm_registered_(false),
      retransmission_alarm_running_(false),
      ack_alarm_registered_(false),
      ack_alarm_time_(QuicTime::Zero()) {
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

bool QuicConnectionHelper::IsWriteBlockedDataBuffered() {
  // Chrome sockets' Write() methods buffer the data until the Write is
  // permitted.
  return true;
}

bool QuicConnectionHelper::IsWriteBlocked(int error) {
  return error == ERR_IO_PENDING;
}

void QuicConnectionHelper::SetRetransmissionAlarm(QuicTime::Delta delay) {
  if (!retransmission_alarm_registered_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&QuicConnectionHelper::OnRetransmissionAlarm,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(delay.ToMicroseconds()));
  }
}

void QuicConnectionHelper::SetAckAlarm(QuicTime::Delta delay) {
  if (!ack_alarm_registered_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&QuicConnectionHelper::OnAckAlarm,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(delay.ToMicroseconds()));
  }
  ack_alarm_registered_ = true;
  ack_alarm_time_ = clock_->Now().Add(delay);
}

void QuicConnectionHelper::ClearAckAlarm() {
  ack_alarm_time_ = QuicTime::Zero();
}

void QuicConnectionHelper::SetSendAlarm(QuicTime alarm_time) {
  send_alarm_registered_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&QuicConnectionHelper::OnSendAlarm,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMicroseconds(
          alarm_time.Subtract(QuicTime::Zero()).ToMicroseconds()));
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

void QuicConnectionHelper::OnRetransmissionAlarm() {
  QuicTime when = connection_->OnRetransmissionTimeout();
  if (!when.IsInitialized()) {
    SetRetransmissionAlarm(clock_->Now().Subtract(when));
  }
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

void QuicConnectionHelper::OnAckAlarm() {
  ack_alarm_registered_ = false;
  // Alarm may have been cleared.
  if (!ack_alarm_time_.IsInitialized()) {
    return;
  }

  // Alarm may have been reset to a later time.
  if (clock_->Now() < ack_alarm_time_) {
    SetAckAlarm(ack_alarm_time_.Subtract(clock_->Now()));
    return;
  }

  ack_alarm_time_ = QuicTime::Zero();
  connection_->SendAck();
}

void QuicConnectionHelper::OnWriteComplete(int result) {
  // TODO(rch): Inform the connection about the result.
  connection_->OnCanWrite();
}

}  // namespace net
