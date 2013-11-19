// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "media/cast/logging/logging_impl.h"

namespace media {
namespace cast {

LoggingImpl::LoggingImpl(base::TickClock* clock,
                         bool enable_data_collection,
                         bool enable_uma_stats,
                         bool enable_tracing)
    : enable_data_collection_(enable_data_collection),
      enable_uma_stats_(enable_uma_stats),
      enable_tracing_(enable_tracing),
      raw_(clock),
      stats_(clock) {}

LoggingImpl::~LoggingImpl() {}

void LoggingImpl::InsertFrameEvent(CastLoggingEvent event,
                                   uint32 rtp_timestamp,
                                   uint32 frame_id) {
  if (enable_data_collection_) {
    raw_.InsertFrameEvent(event, rtp_timestamp, frame_id);
    stats_.InsertFrameEvent(event, rtp_timestamp, frame_id);
  }
  if (enable_tracing_) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT2(event_string.c_str(), "FE",
        TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp, "frame_id",
        frame_id);
  }
}

void LoggingImpl::InsertFrameEventWithSize(CastLoggingEvent event,
                                           uint32 rtp_timestamp,
                                           uint32 frame_id,
                                           int frame_size) {
  if (enable_data_collection_) {
    raw_.InsertFrameEventWithSize(event, rtp_timestamp, frame_id, frame_size);
    stats_.InsertFrameEventWithSize(event, rtp_timestamp, frame_id, frame_size);
  }
  if (enable_uma_stats_) {
    UMA_HISTOGRAM_COUNTS(CastLoggingToString(event), frame_size);
  }
  if (enable_tracing_) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT2(event_string.c_str(), "FES",
        TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp, "frame_size",
        frame_size);
  }

}

void LoggingImpl::InsertFrameEventWithDelay(CastLoggingEvent event,
                                            uint32 rtp_timestamp,
                                            uint32 frame_id,
                                            base::TimeDelta delay) {
  if (enable_data_collection_) {
    raw_.InsertFrameEventWithDelay(event, rtp_timestamp, frame_id, delay);
    stats_.InsertFrameEventWithDelay(event, rtp_timestamp, frame_id, delay);
  }
  if (enable_uma_stats_) {
    UMA_HISTOGRAM_TIMES(CastLoggingToString(event), delay);
  }
   if (enable_tracing_) {
      std::string event_string = CastLoggingToString(event);
      TRACE_EVENT_INSTANT2(event_string.c_str(), "FED",
          TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp, "delay",
          delay.InMilliseconds());
  }
}

void LoggingImpl::InsertPacketEvent(CastLoggingEvent event,
                                    uint32 rtp_timestamp,
                                    uint32 frame_id,
                                    uint16 packet_id,
                                    uint16 max_packet_id,
                                    int size) {
  if (enable_data_collection_) {
    raw_.InsertPacketEvent(event, rtp_timestamp, frame_id, packet_id,
                           max_packet_id, size);
    stats_.InsertPacketEvent(event, rtp_timestamp, frame_id, packet_id,
                             max_packet_id, size);
  }
    if (enable_tracing_) {
      std::string event_string = CastLoggingToString(event);
      TRACE_EVENT_INSTANT2(event_string.c_str(), "PE",
          TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp,
          "packet_id", packet_id);
    }
}

void LoggingImpl::InsertGenericEvent(CastLoggingEvent event, int value) {
  if (enable_data_collection_) {
    raw_.InsertGenericEvent(event, value);
    stats_.InsertGenericEvent(event, value);
  }
  if (enable_uma_stats_) {
    UMA_HISTOGRAM_COUNTS(CastLoggingToString(event), value);
  }
  if (enable_tracing_) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT1(event_string.c_str(), "GE",
        TRACE_EVENT_SCOPE_THREAD, "value", value);
  }
}


// should just get the entire class, would be much easier.
FrameRawMap LoggingImpl::GetFrameRawData() {
  return raw_.GetFrameData();
}

PacketRawMap LoggingImpl::GetPacketRawData() {
 return raw_.GetPacketData();
}

GenericRawMap LoggingImpl::GetGenericRawData() {
 return raw_.GetGenericData();
}

const FrameStatsMap* LoggingImpl::GetFrameStatsData() {
  // Get stats data.
  const FrameStatsMap* stats = stats_.GetFrameStatsData();
  if (enable_uma_stats_) {
    FrameStatsMap::const_iterator it;
    for (it = stats->begin(); it != stats->end(); ++it) {
      // Check for an active event.
      if (it->second->framerate_fps > 0) {
        std::string event_string = CastLoggingToString(it->first);
        UMA_HISTOGRAM_COUNTS(event_string.append("_framerate_fps"),
                             it->second->framerate_fps);
      } else {
        // All active frame events trigger framerate computation.
        continue;
      }
      if (it->second->bitrate_kbps > 0) {
        std::string evnt_string = CastLoggingToString(it->first);
        UMA_HISTOGRAM_COUNTS(evnt_string.append("_bitrate_kbps"),
                             it->second->framerate_fps);
      }
      if (it->second->avg_delay_ms > 0) {
        std::string event_string = CastLoggingToString(it->first);
        UMA_HISTOGRAM_COUNTS(event_string.append("_avg_delay_ms"),
                             it->second->avg_delay_ms);
        UMA_HISTOGRAM_COUNTS(event_string.append("_min_delay_ms"),
                             it->second->min_delay_ms);
        UMA_HISTOGRAM_COUNTS(event_string.append("_max_delay_ms"),
                             it->second->max_delay_ms);
      }
    }
  }
  return stats;
}

const PacketStatsMap* LoggingImpl::GetPacketStatsData() {
  // Get stats data.
  const PacketStatsMap* stats = stats_.GetPacketStatsData();
  if (enable_uma_stats_) {
    PacketStatsMap::const_iterator it;
    for (it = stats->begin(); it != stats->end(); ++it) {
      if (it->second > 0) {
        std::string event_string = CastLoggingToString(it->first);
        UMA_HISTOGRAM_COUNTS(event_string.append("_bitrate_kbps"), it->second);
      }
    }
  }
  return stats;
}

const GenericStatsMap* LoggingImpl::GetGenericStatsData() {
  // Get stats data.
  const GenericStatsMap* stats = stats_.GetGenericStatsData();
   if (enable_uma_stats_) {
    GenericStatsMap::const_iterator it;
    for (it = stats->begin(); it != stats->end(); ++it) {
      if (it->second > 0) {
        UMA_HISTOGRAM_COUNTS(CastLoggingToString(it->first), it->second);
      }
    }
  }
  return stats;
}

void LoggingImpl::Reset() {
  raw_.Reset();
  stats_.Reset();
}

}  // namespace cast
}  // namespace media
