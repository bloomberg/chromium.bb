// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOGGING_STATS_H_
#define MEDIA_CAST_LOGGING_LOGGING_STATS_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/cast/logging/logging_defines.h"

namespace media {
namespace cast {

class LoggingStats {
 public:
  LoggingStats();
  ~LoggingStats();

  void Reset();

  void InsertFrameEvent(const base::TimeTicks& time_of_event,
                        CastLoggingEvent event,
                        uint32 rtp_timestamp,
                        uint32 frame_id);

  void InsertFrameEventWithSize(const base::TimeTicks& time_of_event,
                                CastLoggingEvent event,
                                uint32 rtp_timestamp,
                                uint32 frame_id,
                                int frame_size);

  void InsertFrameEventWithDelay(const base::TimeTicks& time_of_event,
                                 CastLoggingEvent event,
                                 uint32 rtp_timestamp,
                                 uint32 frame_id,
                                 base::TimeDelta delay);

  void InsertPacketEvent(const base::TimeTicks& time_of_event,
                         CastLoggingEvent event,
                         uint32 rtp_timestamp,
                         uint32 frame_id,
                         uint16 packet_id,
                         uint16 max_packet_id,
                         size_t size);

  void InsertGenericEvent(const base::TimeTicks& time_of_event,
                          CastLoggingEvent event, int value);

  FrameStatsMap GetFrameStatsData() const;

  PacketStatsMap GetPacketStatsData() const;

  GenericStatsMap GetGenericStatsData() const;

 private:
  void InsertBaseFrameEvent(const base::TimeTicks& time_of_event,
                            CastLoggingEvent event,
                            uint32 frame_id,
                            uint32 rtp_timestamp);

  FrameStatsMap frame_stats_;
  PacketStatsMap packet_stats_;
  GenericStatsMap generic_stats_;

  DISALLOW_COPY_AND_ASSIGN(LoggingStats);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_STATS_H_

