// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOGGING_STATS_H_
#define MEDIA_CAST_LOGGING_LOGGING_STATS_H_

#include "base/basictypes.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/logging/logging_defines.h"

namespace media {
namespace cast {

class LoggingStats {
 public:
  explicit LoggingStats(base::TickClock* clock);

  ~LoggingStats();

  void Reset();

  void InsertFrameEvent(CastLoggingEvent event,
                        uint32 rtp_timestamp,
                        uint32 frame_id);

  void InsertFrameEventWithSize(CastLoggingEvent event,
                                uint32 rtp_timestamp,
                                uint32 frame_id,
                                int frame_size);

  void InsertFrameEventWithDelay(CastLoggingEvent event,
                                 uint32 rtp_timestamp,
                                 uint32 frame_id,
                                 base::TimeDelta delay);

  void InsertPacketEvent(CastLoggingEvent event,
                         uint32 rtp_timestamp,
                         uint32 frame_id,
                         uint16 packet_id,
                         uint16 max_packet_id,
                         size_t size);

  void InsertGenericEvent(CastLoggingEvent event, int value);

  // Get log stats: some of the values, such as frame rate and bit rates are
  // computed at the time of the call.
  const FrameStatsMap* GetFrameStatsData();

  const PacketStatsMap* GetPacketStatsData();

  const GenericStatsMap* GetGenericStatsData();

 private:
  void InsertBaseFrameEvent(CastLoggingEvent event,
                            uint32 frame_id,
                            uint32 rtp_timestamp);
  FrameStatsMap frame_stats_;
  PacketStatsMap packet_stats_;
  GenericStatsMap generic_stats_;
  // Every event has an individual start time
  base::TimeTicks start_time_[kNumOfLoggingEvents];
  // Keep track of event counts.
  int counts_[kNumOfLoggingEvents];
  base::TickClock* const clock_;  // Not owned by this class.

  DISALLOW_COPY_AND_ASSIGN(LoggingStats);
 };

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_STATS_H_

