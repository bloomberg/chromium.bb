// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_PACING_PACED_SENDER_H_
#define MEDIA_CAST_PACING_PACED_SENDER_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"

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

 protected:
  virtual ~PacedPacketSender() {}
};

class PacedSender : public PacedPacketSender {
 public:
  explicit PacedSender(PacketSender* transport);
  virtual ~PacedSender();

  // Returns the time when the pacer want a worker thread to call Process.
  base::TimeTicks TimeNextProcess();

  // Process any pending packets in the queue(s).
  void Process();

  virtual bool SendPacket(const std::vector<uint8>& packet,
                          int num_of_packets) OVERRIDE;

  virtual bool ResendPacket(const std::vector<uint8>& packet,
                            int num_of_packets) OVERRIDE;

  virtual bool SendRtcpPacket(const std::vector<uint8>& packet) OVERRIDE;

  void set_clock(base::TickClock* clock) {
    clock_ = clock;
  }

 private:
  void SendStoredPacket();
  void UpdateBurstSize(int num_of_packets);

  typedef std::list<std::vector<uint8> > PacketList;

  int burst_size_;
  int packets_sent_in_burst_;
  base::TimeTicks time_last_process_;
  PacketList packet_list_;
  PacketList resend_packet_list_;
  PacketSender* transport_;

  scoped_ptr<base::TickClock> default_tick_clock_;
  base::TickClock* clock_;

  DISALLOW_COPY_AND_ASSIGN(PacedSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_PACING_PACED_SENDER_H_
