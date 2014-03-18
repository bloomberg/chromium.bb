// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_PACING_PACED_SENDER_H_
#define MEDIA_CAST_TRANSPORT_PACING_PACED_SENDER_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/transport/udp_transport.h"

namespace media {
namespace cast {

class LoggingImpl;

namespace transport {

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
  // The |external_transport| should only be used by the Cast receiver and for
  // testing.
  PacedSender(
      base::TickClock* clock,
      LoggingImpl* logging,
      PacketSender* external_transport,
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner);

  virtual ~PacedSender();

  // These must be called before non-RTCP packets are sent.
  void RegisterAudioSsrc(uint32 audio_ssrc);
  void RegisterVideoSsrc(uint32 video_ssrc);

  // PacedPacketSender implementation.
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
                              PacketList* packets_not_sent,
                              bool retransmit);

  // Actually sends the packets to the transport.
  bool TransmitPackets(const PacketList& packets);
  void SendStoredPackets();
  void UpdateBurstSize(size_t num_of_packets);

  void LogPacketEvent(const Packet& packet, bool retransmit);

  base::TickClock* const clock_;  // Not owned by this class.
  LoggingImpl* const logging_;    // Not owned by this class.
  PacketSender* transport_;       // Not owned by this class.
  scoped_refptr<base::SingleThreadTaskRunner> transport_task_runner_;
  uint32 audio_ssrc_;
  uint32 video_ssrc_;
  size_t burst_size_;
  size_t packets_sent_in_burst_;
  base::TimeTicks time_last_process_;
  // Note: We can't combine the |packet_list_| and the |resend_packet_list_|
  // since then we might get reordering of the retransmitted packets.
  PacketList packet_list_;
  PacketList resend_packet_list_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PacedSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PacedSender);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_PACING_PACED_SENDER_H_
