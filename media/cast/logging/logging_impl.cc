// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "media/cast/logging/logging_impl.h"
#include "net/base/big_endian.h"

namespace media {
namespace cast {

LoggingImpl::LoggingImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_proxy,
    const CastLoggingConfig& config)
    : main_thread_proxy_(main_thread_proxy),
      config_(config),
      raw_(),
      stats_() {}

LoggingImpl::~LoggingImpl() {}

void LoggingImpl::InsertFrameEvent(const base::TimeTicks& time_of_event,
                                   CastLoggingEvent event, uint32 rtp_timestamp,
                                   uint32 frame_id) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_raw_data_collection) {
    raw_.InsertFrameEvent(time_of_event, event, rtp_timestamp, frame_id);
  }
  if (config_.enable_stats_data_collection) {
    stats_.InsertFrameEvent(time_of_event, event, rtp_timestamp, frame_id);
  }
  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT2(event_string.c_str(), "FE", TRACE_EVENT_SCOPE_THREAD,
                         "rtp_timestamp", rtp_timestamp, "frame_id", frame_id);
  }
}

void LoggingImpl::InsertFrameEventWithSize(const base::TimeTicks& time_of_event,
                                           CastLoggingEvent event,
                                           uint32 rtp_timestamp,
                                           uint32 frame_id, int frame_size) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_raw_data_collection) {
    raw_.InsertFrameEventWithSize(time_of_event, event, rtp_timestamp, frame_id,
                                  frame_size);
  }
  if (config_.enable_stats_data_collection) {
    stats_.InsertFrameEventWithSize(time_of_event, event, rtp_timestamp,
                                    frame_id, frame_size);
  }

  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT2(event_string.c_str(), "FES", TRACE_EVENT_SCOPE_THREAD,
                         "rtp_timestamp", rtp_timestamp, "frame_size",
                         frame_size);
  }
}

void LoggingImpl::InsertFrameEventWithDelay(
    const base::TimeTicks& time_of_event, CastLoggingEvent event,
    uint32 rtp_timestamp, uint32 frame_id, base::TimeDelta delay) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_raw_data_collection) {
    raw_.InsertFrameEventWithDelay(time_of_event, event, rtp_timestamp,
                                   frame_id, delay);
  }
  if (config_.enable_stats_data_collection) {
    stats_.InsertFrameEventWithDelay(time_of_event, event, rtp_timestamp,
                                     frame_id, delay);
  }

  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT2(event_string.c_str(), "FED", TRACE_EVENT_SCOPE_THREAD,
                         "rtp_timestamp", rtp_timestamp, "delay",
                         delay.InMilliseconds());
  }
}

void LoggingImpl::InsertPacketListEvent(const base::TimeTicks& time_of_event,
                                        CastLoggingEvent event,
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
    InsertPacketEvent(time_of_event, event, rtp_timestamp, kFrameIdUnknown,
                      packet_id, max_packet_id, packet.size());
  }
}

void LoggingImpl::InsertPacketEvent(const base::TimeTicks& time_of_event,
                                    CastLoggingEvent event,
                                    uint32 rtp_timestamp, uint32 frame_id,
                                    uint16 packet_id, uint16 max_packet_id,
                                    size_t size) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_raw_data_collection) {
    raw_.InsertPacketEvent(time_of_event, event, rtp_timestamp, frame_id,
                           packet_id, max_packet_id, size);
  }
  if (config_.enable_stats_data_collection) {
    stats_.InsertPacketEvent(time_of_event, event, rtp_timestamp, frame_id,
                             packet_id, max_packet_id, size);
  }
  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT2(event_string.c_str(), "PE", TRACE_EVENT_SCOPE_THREAD,
                         "rtp_timestamp", rtp_timestamp, "packet_id",
                         packet_id);
  }
}

void LoggingImpl::InsertGenericEvent(const base::TimeTicks& time_of_event,
                                     CastLoggingEvent event, int value) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_raw_data_collection) {
    raw_.InsertGenericEvent(time_of_event, event, value);
  }
  if (config_.enable_stats_data_collection) {
    stats_.InsertGenericEvent(time_of_event, event, value);
  }

  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT1(event_string.c_str(), "GE", TRACE_EVENT_SCOPE_THREAD,
                         "value", value);
  }
}

void LoggingImpl::AddRawEventSubscriber(RawEventSubscriber* subscriber) {
  raw_.AddSubscriber(subscriber);
}

void LoggingImpl::RemoveRawEventSubscriber(RawEventSubscriber* subscriber) {
  raw_.RemoveSubscriber(subscriber);
}

FrameStatsMap LoggingImpl::GetFrameStatsData() const {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  return stats_.GetFrameStatsData();
}

PacketStatsMap LoggingImpl::GetPacketStatsData() const {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  return stats_.GetPacketStatsData();
}

GenericStatsMap LoggingImpl::GetGenericStatsData() const {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  return stats_.GetGenericStatsData();
}

void LoggingImpl::ResetStats() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  stats_.Reset();
}

}  // namespace cast
}  // namespace media
