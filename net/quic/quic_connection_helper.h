// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Chrome-specific helper for QuicConnection which uses
// a TaskRunner for alarms, and uses a DatagramClientSocket for writing data.

#ifndef NET_QUIC_QUIC_CONNECTION_HELPER_H_
#define NET_QUIC_QUIC_CONNECTION_HELPER_H_

#include "net/quic/quic_connection.h"

#include <set>

#include "base/memory/weak_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"
#include "net/udp/datagram_client_socket.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace net {

class QuicClock;
class QuicRandom;

namespace test {
class QuicConnectionHelperPeer;
}  // namespace test

class NET_EXPORT_PRIVATE QuicConnectionHelper
    : public QuicConnectionHelperInterface {
 public:
  QuicConnectionHelper(base::TaskRunner* task_runner,
                       const QuicClock* clock,
                       QuicRandom* random_generator,
                       DatagramClientSocket* socket);

  virtual ~QuicConnectionHelper();

  // QuicConnectionHelperInterface
  virtual void SetConnection(QuicConnection* connection) OVERRIDE;
  virtual const QuicClock* GetClock() const OVERRIDE;
  virtual QuicRandom* GetRandomGenerator() OVERRIDE;
  virtual int WritePacketToWire(const QuicEncryptedPacket& packet,
                                int* error) OVERRIDE;
  virtual bool IsWriteBlockedDataBuffered() OVERRIDE;
  virtual bool IsWriteBlocked(int error) OVERRIDE;
  virtual void SetRetransmissionAlarm(QuicTime::Delta delay) OVERRIDE;
  virtual void SetSendAlarm(QuicTime alarm_time) OVERRIDE;
  virtual void SetTimeoutAlarm(QuicTime::Delta delay) OVERRIDE;
  virtual bool IsSendAlarmSet() OVERRIDE;
  virtual void UnregisterSendAlarmIfRegistered() OVERRIDE;
  virtual void SetAckAlarm(QuicTime::Delta delay) OVERRIDE;
  virtual void ClearAckAlarm() OVERRIDE;

 private:
  friend class test::QuicConnectionHelperPeer;

  // An alarm is scheduled for each data-bearing packet as it is sent out.
  // When the alarm goes off, the connection checks to see if the packet has
  // been acked, and retransmits if it has not.
  void OnRetransmissionAlarm();
  // An alarm that is scheduled when the sent scheduler requires a
  // a delay before sending packets and fires when the packet may be sent.
  void OnSendAlarm();
  // An alarm which fires when the connection may have timed out.
  void OnTimeoutAlarm();
  // A completion callback invoked when a write completes.
  void OnWriteComplete(int result);
  // An alarm which fires if we've hit a timeout on sending an ack.
  void OnAckAlarm();

  base::WeakPtrFactory<QuicConnectionHelper> weak_factory_;
  base::TaskRunner* task_runner_;
  DatagramClientSocket* socket_;
  QuicConnection* connection_;
  const QuicClock* clock_;
  QuicRandom* random_generator_;
  bool send_alarm_registered_;
  bool timeout_alarm_registered_;
  bool retransmission_alarm_registered_;
  bool retransmission_alarm_running_;
  bool ack_alarm_registered_;
  QuicTime ack_alarm_time_;
  // Times that packets should be retransmitted.
  std::map<QuicPacketSequenceNumber, QuicTime> retransmission_times_;

  DISALLOW_COPY_AND_ASSIGN(QuicConnectionHelper);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_HELPER_H_
