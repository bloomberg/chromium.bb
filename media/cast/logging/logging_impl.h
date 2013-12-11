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

#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "media/cast/cast_config.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/logging_raw.h"
#include "media/cast/logging/logging_stats.h"

namespace media {
namespace cast {

// Should only be called from the main thread.
class LoggingImpl : public base::NonThreadSafe {
 public:
  LoggingImpl(base::TickClock* clock,
              scoped_refptr<base::TaskRunner> main_thread_proxy,
              const CastLoggingConfig& config);

  ~LoggingImpl();

  // TODO(pwestin): Add argument to API to send in time of event instead of
  // grabbing now.
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
  void InsertPacketListEvent(CastLoggingEvent event, const PacketList& packets);

  void InsertPacketEvent(CastLoggingEvent event,
                         uint32 rtp_timestamp,
                         uint32 frame_id,
                         uint16 packet_id,
                         uint16 max_packet_id,
                         size_t size);
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
  scoped_refptr<base::TaskRunner> main_thread_proxy_;
  const CastLoggingConfig config_;
  LoggingRaw raw_;
  LoggingStats stats_;

  DISALLOW_COPY_AND_ASSIGN(LoggingImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_IMPL_H_
