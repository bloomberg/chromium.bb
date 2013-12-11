// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_PACING_PACED_SENDER_H_
#define MEDIA_CAST_NET_PACING_PACED_SENDER_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"

namespace media {
namespace cast {

// We have this pure virtual class to enable mocking.
class PacedPacketSender {
 public:
  // Inform the pacer / sender of the total number of packets.
  virtual bool SendPackets(const PacketList& packets) = 0;

  virtual bool ResendPackets(const PacketList& packets) = 0;

  virtual bool SendRtcpPacket(const Packet& packet) = 0;

  virtual ~PacedPacketSender() {}
};

class PacedSender : public PacedPacketSender,
                    public base::NonThreadSafe,
                    public base::SupportsWeakPtr<PacedSender> {
 public:
  PacedSender(scoped_refptr<CastEnvironment> cast_environment,
              PacketSender* transport);
  virtual ~PacedSender();

  virtual bool SendPackets(const PacketList& packets) OVERRIDE;

  virtual bool ResendPackets(const PacketList& packets) OVERRIDE;

  virtual bool SendRtcpPacket(const Packet& packet) OVERRIDE;

 protected:
  // Schedule a delayed task on the main cast thread when it's time to send the
  // next packet burst.
  void ScheduleNextSend();

  // Process any pending packets in the queue(s).
  void SendNextPacketBurst();

 private:
  bool SendPacketsToTransport(const PacketList& packets,
                              PacketList* packets_not_sent);
  void SendStoredPackets();
  void UpdateBurstSize(size_t num_of_packets);

  scoped_refptr<CastEnvironment> cast_environment_;
  size_t burst_size_;
  size_t packets_sent_in_burst_;
  base::TimeTicks time_last_process_;
  // Note: We can't combine the |packet_list_| and the |resend_packet_list_|
  // since then we might get reordering of the retransmitted packets.
  PacketList packet_list_;
  PacketList resend_packet_list_;
  PacketSender* transport_;

  base::WeakPtrFactory<PacedSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PacedSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_PACING_PACED_SENDER_H_
