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
  virtual bool SendRtcpPacket(PacketRef packet) = 0;

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
  virtual bool SendRtcpPacket(PacketRef packet) OVERRIDE;

 private:
  // Actually sends the packets to the transport.
  void SendStoredPackets();
  void LogPacketEvent(const Packet& packet, bool retransmit);

  enum PacketType {
    PacketType_RTCP,
    PacketType_Resend,
    PacketType_Normal
  };
  enum State {
    // In an unblocked state, we can send more packets.
    // We have to check the current time against |burst_end_| to see if we are
    // appending to the current burst or if we can start a new one.
    State_Unblocked,
    // In this state, we are waiting for a callback from the udp transport.
    // This happens when the OS-level buffer is full. Once we receive the
    // callback, we go to State_Unblocked and see if we can write more packets
    // to the current burst. (Or the next burst if enough time has passed.)
    State_TransportBlocked,
    // Once we've written enough packets for a time slice, we go into this
    // state and PostDelayTask a call to ourselves to wake up when we can
    // send more data.
    State_BurstFull
  };

  bool empty() const;
  size_t size() const;

  // Returns the next packet to send. RTCP packets have highest priority,
  // resend packets have second highest priority and then comes everything
  // else.
  PacketRef GetNextPacket(PacketType* packet_type);

  base::TickClock* const clock_;  // Not owned by this class.
  LoggingImpl* const logging_;    // Not owned by this class.
  PacketSender* transport_;       // Not owned by this class.
  scoped_refptr<base::SingleThreadTaskRunner> transport_task_runner_;
  uint32 audio_ssrc_;
  uint32 video_ssrc_;
  // Note: We can't combine the |packet_list_| and the |resend_packet_list_|
  // since then we might get reordering of the retransmitted packets.
  std::deque<PacketRef> rtcp_packet_list_;
  std::deque<PacketRef> resend_packet_list_;
  std::deque<PacketRef> packet_list_;

  // Maximum burst size for the next three bursts.
  size_t max_burst_size_;
  size_t next_max_burst_size_;
  size_t next_next_max_burst_size_;
  // Number of packets already sent in the current burst.
  size_t current_burst_size_;
  // This is when the current burst ends.
  base::TimeTicks burst_end_;

  State state_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PacedSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PacedSender);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_PACING_PACED_SENDER_H_
