// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_CAST_LOGGING_LOGGING_IMPL_H_
#define MEDIA_CAST_LOGGING_LOGGING_IMPL_H_

// Generic class that handles event logging for the cast library.
// Logging has three possible optional forms:
// 1. Raw data and stats accessible by the application.
// 2. UMA stats.
// 3. Tracing of raw events.

#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/logging_raw.h"
#include "media/cast/logging/logging_stats.h"

namespace media {
namespace cast {

class LoggingImpl {
 public:
  LoggingImpl(base::TickClock* clock,
              bool enable_data_collection,
              bool enable_uma_stats,
              bool enable_tracing);

  ~LoggingImpl();

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
                         int size);
  void InsertGenericEvent(CastLoggingEvent event, int value);

  // Get raw data.
  FrameRawMap GetFrameRawData();
  PacketRawMap GetPacketRawData();
  GenericRawMap GetGenericRawData();
  // Get stats only (computed when called). Triggers UMA stats when enabled.
  const FrameStatsMap* GetFrameStatsData();
  const PacketStatsMap* GetPacketStatsData();
  const GenericStatsMap* GetGenericStatsData();

  void Reset();

 private:
  LoggingRaw raw_;
  LoggingStats stats_;
  bool enable_data_collection_;
  bool enable_uma_stats_;
  bool enable_tracing_;

  DISALLOW_COPY_AND_ASSIGN(LoggingImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_IMPL_H_
