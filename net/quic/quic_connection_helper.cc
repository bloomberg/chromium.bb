// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_helper.h"

#include "base/location.h"
#include "base/logging.h"
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
        task_posted_(false),
        weak_factory_(this) {}

 protected:
  virtual void SetImpl() OVERRIDE {
    DCHECK(deadline().IsInitialized());
    if (task_posted_) {
      // Since tasks can not be un-posted, OnAlarm will be invoked which
      // will notice that deadline has not yet been reached, and will set
      // the alarm for the new deadline.
      return;
    }

    int64 delay_us = deadline().Subtract(clock_->Now()).ToMicroseconds();
    if (delay_us < 0) {
      delay_us = 0;
    }
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&QuicChromeAlarm::OnAlarm, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(delay_us));
    task_posted_ = true;
  }

  virtual void CancelImpl() OVERRIDE {
    DCHECK(!deadline().IsInitialized());
    // Since tasks can not be un-posted, OnAlarm will be invoked which
    // will notice that deadline is not Initialized and will do nothing.
  }

 private:
  void OnAlarm() {
    DCHECK(task_posted_);
    task_posted_ = false;
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
  bool task_posted_;
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
  int rv = socket_->Write(buf.get(),
                          packet.length(),
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

QuicAlarm* QuicConnectionHelper::CreateAlarm(QuicAlarm::Delegate* delegate) {
  return new QuicChromeAlarm(clock_, task_runner_, delegate);
}

void QuicConnectionHelper::OnWriteComplete(int result) {
  // TODO(rch): Inform the connection about the result.
  connection_->OnCanWrite();
}

}  // namespace net
