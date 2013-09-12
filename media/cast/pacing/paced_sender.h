// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_PACING_PACED_SENDER_H_
#define MEDIA_CAST_PACING_PACED_SENDER_H_

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
#include "media/cast/cast_thread.h"

namespace media {
namespace cast {

class PacedPacketSender {
 public:
  // Inform the pacer / sender of the total number of packets.
  virtual bool SendPacket(const std::vector<uint8>& packet,
                          int num_of_packets) = 0;

  virtual bool ResendPacket(const std::vector<uint8>& packet,
                            int num_of_packets) = 0;

  virtual bool SendRtcpPacket(const std::vector<uint8>& packet) = 0;

  virtual ~PacedPacketSender() {}
};

class PacedSender : public PacedPacketSender,
                    public base::NonThreadSafe,
                    public base::SupportsWeakPtr<PacedSender> {
 public:
  PacedSender(scoped_refptr<CastThread> cast_thread, PacketSender* transport);
  virtual ~PacedSender();

  virtual bool SendPacket(const std::vector<uint8>& packet,
                          int num_of_packets) OVERRIDE;

  virtual bool ResendPacket(const std::vector<uint8>& packet,
                            int num_of_packets) OVERRIDE;

  virtual bool SendRtcpPacket(const std::vector<uint8>& packet) OVERRIDE;

  void set_clock(base::TickClock* clock) {
    clock_ = clock;
  }

 protected:
  // Schedule a delayed task on the main cast thread when it's time to send the
  // next packet burst.
  void ScheduleNextSend();

  // Process any pending packets in the queue(s).
  void SendNextPacketBurst();

 private:
  void SendStoredPacket();
  void UpdateBurstSize(int num_of_packets);

  typedef std::list<std::vector<uint8> > PacketList;

  scoped_refptr<CastThread> cast_thread_;
  int burst_size_;
  int packets_sent_in_burst_;
  base::TimeTicks time_last_process_;
  PacketList packet_list_;
  PacketList resend_packet_list_;
  PacketSender* transport_;

  base::DefaultTickClock default_tick_clock_;
  base::TickClock* clock_;

  base::WeakPtrFactory<PacedSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PacedSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_PACING_PACED_SENDER_H_
