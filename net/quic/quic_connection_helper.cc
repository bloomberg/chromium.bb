// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_helper.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_utils.h"

namespace net {

namespace {

class QuicChromeAlarm : public QuicAlarm {
 public:
  QuicChromeAlarm(const QuicClock* clock,
                  base::TaskRunner* task_runner,
                  QuicAlarm::Delegate* delegate)
      : QuicAlarm(delegate),
        clock_(clock),
        task_runner_(task_runner),
        task_deadline_(QuicTime::Zero()),
        weak_factory_(this) {}

 protected:
  virtual void SetImpl() OVERRIDE {
    DCHECK(deadline().IsInitialized());
    if (task_deadline_.IsInitialized()) {
      if (task_deadline_ <= deadline()) {
        // Since tasks can not be un-posted, OnAlarm will be invoked which
        // will notice that deadline has not yet been reached, and will set
        // the alarm for the new deadline.
        return;
      }
      // The scheduled task is after new deadline.  Invalidate the weak ptrs
      // so that task does not execute when we're not expecting it.
      weak_factory_.InvalidateWeakPtrs();
    }

    int64 delay_us = deadline().Subtract(clock_->Now()).ToMicroseconds();
    if (delay_us < 0) {
      delay_us = 0;
    }
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&QuicChromeAlarm::OnAlarm, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(delay_us));
    task_deadline_ = deadline();
  }

  virtual void CancelImpl() OVERRIDE {
    DCHECK(!deadline().IsInitialized());
    // Since tasks can not be un-posted, OnAlarm will be invoked which
    // will notice that deadline is not Initialized and will do nothing.
  }

 private:
  void OnAlarm() {
    DCHECK(task_deadline_.IsInitialized());
    task_deadline_ = QuicTime::Zero();
    // The alarm may have been cancelled.
    if (!deadline().IsInitialized()) {
      return;
    }

    // The alarm may have been re-set to a later time.
    if (clock_->Now() < deadline()) {
      SetImpl();
      return;
    }

    Fire();
  }

  const QuicClock* clock_;
  base::TaskRunner* task_runner_;
  // If a task has been posted to the message loop, this is the time it
  // was scheduled to fire.  Tracking this allows us to avoid posting a
  // new tast if the new deadline is in the future, but permits us to
  // post a new task when the new deadline now earlier than when
  // previously posted.
  QuicTime task_deadline_;
  base::WeakPtrFactory<QuicChromeAlarm> weak_factory_;
};

}  // namespace

QuicConnectionHelper::QuicConnectionHelper(base::TaskRunner* task_runner,
                                           const QuicClock* clock,
                                           QuicRandom* random_generator,
                                           DatagramClientSocket* socket)
    : weak_factory_(this),
      task_runner_(task_runner),
      socket_(socket),
      clock_(clock),
      random_generator_(random_generator) {
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

WriteResult QuicConnectionHelper::WritePacketToWire(
    const QuicEncryptedPacket& packet) {
  if (connection_->ShouldSimulateLostPacket()) {
    DLOG(INFO) << "Dropping packet due to fake packet loss.";
    return WriteResult(WRITE_STATUS_OK, packet.length());
  }

  scoped_refptr<StringIOBuffer> buf(
      new StringIOBuffer(std::string(packet.data(),
                                     packet.length())));
  int rv = socket_->Write(buf.get(),
                          packet.length(),
                          base::Bind(&QuicConnectionHelper::OnWriteComplete,
                                     weak_factory_.GetWeakPtr()));
  WriteStatus status = WRITE_STATUS_OK;
  if (rv < 0) {
    if (rv != ERR_IO_PENDING) {
      UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.WriteError", -rv);
      status = WRITE_STATUS_ERROR;
    } else {
      status = WRITE_STATUS_BLOCKED;
    }
  }

  return WriteResult(status, rv);
}

bool QuicConnectionHelper::IsWriteBlockedDataBuffered() {
  // Chrome sockets' Write() methods buffer the data until the Write is
  // permitted.
  return true;
}

QuicAlarm* QuicConnectionHelper::CreateAlarm(QuicAlarm::Delegate* delegate) {
  return new QuicChromeAlarm(clock_, task_runner_, delegate);
}

void QuicConnectionHelper::OnWriteComplete(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  WriteResult result(rv < 0 ? WRITE_STATUS_ERROR : WRITE_STATUS_OK, rv);
  connection_->OnPacketSent(result);
  connection_->OnCanWrite();
}

}  // namespace net
