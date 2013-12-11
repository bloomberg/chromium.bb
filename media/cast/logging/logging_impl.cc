// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "media/cast/logging/logging_impl.h"
#include "net/base/big_endian.h"

namespace media {
namespace cast {

LoggingImpl::LoggingImpl(base::TickClock* clock,
                         scoped_refptr<base::TaskRunner> main_thread_proxy,
                         const CastLoggingConfig& config)
    : main_thread_proxy_(main_thread_proxy),
      config_(config),
      raw_(clock),
      stats_(clock) {}

LoggingImpl::~LoggingImpl() {}

void LoggingImpl::InsertFrameEvent(CastLoggingEvent event,
                                   uint32 rtp_timestamp,
                                   uint32 frame_id) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_data_collection) {
    raw_.InsertFrameEvent(event, rtp_timestamp, frame_id);
    stats_.InsertFrameEvent(event, rtp_timestamp, frame_id);
  }
  if (config_.enable_tracing) {
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
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_data_collection) {
    raw_.InsertFrameEventWithSize(event, rtp_timestamp, frame_id, frame_size);
    stats_.InsertFrameEventWithSize(event, rtp_timestamp, frame_id, frame_size);
  }
  if (config_.enable_uma_stats) {
    UMA_HISTOGRAM_COUNTS(CastLoggingToString(event), frame_size);
  }
  if (config_.enable_tracing) {
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
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_data_collection) {
    raw_.InsertFrameEventWithDelay(event, rtp_timestamp, frame_id, delay);
    stats_.InsertFrameEventWithDelay(event, rtp_timestamp, frame_id, delay);
  }
  if (config_.enable_uma_stats) {
    UMA_HISTOGRAM_TIMES(CastLoggingToString(event), delay);
  }
  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT2(event_string.c_str(), "FED",
        TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp, "delay",
        delay.InMilliseconds());
  }
}

void LoggingImpl::InsertPacketListEvent(CastLoggingEvent event,
                                        const PacketList& packets) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  for (unsigned int i = 0; i < packets.size(); ++i) {
    const Packet& packet = packets[i];
    // Parse basic properties.
    uint32 rtp_timestamp;
    uint16 packet_id, max_packet_id;
    const uint8* packet_data = &packet[0];
    net::BigEndianReader big_endian_reader(packet_data + 4, 4);
    big_endian_reader.ReadU32(&rtp_timestamp);
    net::BigEndianReader cast_big_endian_reader(packet_data + 12 + 2, 4);
    cast_big_endian_reader.ReadU16(&packet_id);
    cast_big_endian_reader.ReadU16(&max_packet_id);
    // rtp_timestamp is enough - no need for frame_id as well.
    InsertPacketEvent(event, rtp_timestamp, kFrameIdUnknown, packet_id,
                      max_packet_id, packet.size());
  }
}

void LoggingImpl::InsertPacketEvent(CastLoggingEvent event,
                                    uint32 rtp_timestamp,
                                    uint32 frame_id,
                                    uint16 packet_id,
                                    uint16 max_packet_id,
                                    size_t size) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_data_collection) {
    raw_.InsertPacketEvent(event, rtp_timestamp, frame_id, packet_id,
                           max_packet_id, size);
    stats_.InsertPacketEvent(event, rtp_timestamp, frame_id, packet_id,
                             max_packet_id, size);
  }
  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT2(event_string.c_str(), "PE",
        TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp,
        "packet_id", packet_id);
  }
}

void LoggingImpl::InsertGenericEvent(CastLoggingEvent event, int value) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_data_collection) {
    raw_.InsertGenericEvent(event, value);
    stats_.InsertGenericEvent(event, value);
  }
  if (config_.enable_uma_stats) {
    UMA_HISTOGRAM_COUNTS(CastLoggingToString(event), value);
  }
  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT1(event_string.c_str(), "GE",
        TRACE_EVENT_SCOPE_THREAD, "value", value);
  }
}

// should just get the entire class, would be much easier.
FrameRawMap LoggingImpl::GetFrameRawData() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  return raw_.GetFrameData();
}

PacketRawMap LoggingImpl::GetPacketRawData() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  return raw_.GetPacketData();
}

GenericRawMap LoggingImpl::GetGenericRawData() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  return raw_.GetGenericData();
}

const FrameStatsMap* LoggingImpl::GetFrameStatsData() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  // Get stats data.
  const FrameStatsMap* stats = stats_.GetFrameStatsData();
  if (config_.enable_uma_stats) {
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
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  // Get stats data.
  const PacketStatsMap* stats = stats_.GetPacketStatsData();
  if (config_.enable_uma_stats) {
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
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  // Get stats data.
  const GenericStatsMap* stats = stats_.GetGenericStatsData();
  if (config_.enable_uma_stats) {
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
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  raw_.Reset();
  stats_.Reset();
}

}  // namespace cast
}  // namespace media
