// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "media/cast/logging/logging_impl.h"
#include "net/base/big_endian.h"

namespace media {
namespace cast {

LoggingImpl::LoggingImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_proxy,
    const CastLoggingConfig& config)
    : main_thread_proxy_(main_thread_proxy),
      config_(config),
      raw_(config.is_sender),
      stats_() {}

LoggingImpl::~LoggingImpl() {}

void LoggingImpl::InsertFrameEvent(const base::TimeTicks& time_of_event,
                                   CastLoggingEvent event,
                                   uint32 rtp_timestamp,
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
    TRACE_EVENT_INSTANT2(event_string.c_str(), "FE",
        TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp, "frame_id",
        frame_id);
  }
}

void LoggingImpl::InsertFrameEventWithSize(const base::TimeTicks& time_of_event,
                                           CastLoggingEvent event,
                                           uint32 rtp_timestamp,
                                           uint32 frame_id,
                                           int frame_size) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_raw_data_collection) {
    raw_.InsertFrameEventWithSize(time_of_event, event, rtp_timestamp, frame_id,
                                  frame_size);
  }
  if (config_.enable_stats_data_collection) {
    stats_.InsertFrameEventWithSize(time_of_event, event, rtp_timestamp,
                                    frame_id, frame_size);
  }
  if (config_.enable_uma_stats) {
    if (event == kAudioFrameEncoded)
      UMA_HISTOGRAM_COUNTS("Cast.AudioFrameEncoded", frame_size);
    else if (event == kVideoFrameEncoded) {
      UMA_HISTOGRAM_COUNTS("Cast.VideoFrameEncoded", frame_size);
    }
  }

  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT2(event_string.c_str(), "FES",
        TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp, "frame_size",
        frame_size);
  }
}

void LoggingImpl::InsertFrameEventWithDelay(
    const base::TimeTicks& time_of_event,
    CastLoggingEvent event,
    uint32 rtp_timestamp,
    uint32 frame_id,
    base::TimeDelta delay) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  if (config_.enable_raw_data_collection) {
    raw_.InsertFrameEventWithDelay(time_of_event, event, rtp_timestamp,
                                   frame_id, delay);
  }
  if (config_.enable_stats_data_collection) {
    stats_.InsertFrameEventWithDelay(time_of_event, event, rtp_timestamp,
                                     frame_id, delay);
  }
  if (config_.enable_uma_stats) {
    if (event == kAudioPlayoutDelay)
      UMA_HISTOGRAM_TIMES("Cast.AudioPlayoutDelay", delay);
    else if (event == kVideoRenderDelay) {
      UMA_HISTOGRAM_TIMES("Cast.VideoRenderDelay", delay);
    }
  }
  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT2(event_string.c_str(), "FED",
        TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp, "delay",
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
                                    uint32 rtp_timestamp,
                                    uint32 frame_id,
                                    uint16 packet_id,
                                    uint16 max_packet_id,
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
    TRACE_EVENT_INSTANT2(event_string.c_str(), "PE",
        TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp", rtp_timestamp,
        "packet_id", packet_id);
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
  if (config_.enable_uma_stats) {
    InsertGenericUmaEvent(event, value);
  }
  if (config_.enable_tracing) {
    std::string event_string = CastLoggingToString(event);
    TRACE_EVENT_INSTANT1(event_string.c_str(), "GE",
        TRACE_EVENT_SCOPE_THREAD, "value", value);
  }
}

void LoggingImpl::InsertGenericUmaEvent(CastLoggingEvent event, int value) {
  switch(event) {
    case kRttMs:
      UMA_HISTOGRAM_COUNTS("Cast.RttMs", value);
      break;
    case kPacketLoss:
      UMA_HISTOGRAM_COUNTS("Cast.PacketLoss", value);
      break;
    case kJitterMs:
      UMA_HISTOGRAM_COUNTS("Cast.JitterMs", value);
      break;
    case kRembBitrate:
      UMA_HISTOGRAM_COUNTS("Cast.RembBitrate", value);
      break;
    default:
      // No-op
      break;
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

AudioRtcpRawMap LoggingImpl::GetAudioRtcpRawData() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  return raw_.GetAndResetAudioRtcpData();
}

VideoRtcpRawMap LoggingImpl::GetVideoRtcpRawData() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  return raw_.GetAndResetVideoRtcpData();
}

const FrameStatsMap* LoggingImpl::GetFrameStatsData(
    const base::TimeTicks& now) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  // Get stats data.
  const FrameStatsMap* stats = stats_.GetFrameStatsData(now);
  if (config_.enable_uma_stats) {
    FrameStatsMap::const_iterator it;
    for (it = stats->begin(); it != stats->end(); ++it) {
      // Check for an active event.
      // The default frame event implies frame rate.
      if (it->second->framerate_fps > 0) {
        switch (it->first) {
          case kAudioFrameReceived:
            UMA_HISTOGRAM_COUNTS("Cast.Stats.AudioFrameReceived",
                it->second->framerate_fps);
            break;
          case kAudioFrameCaptured:
            UMA_HISTOGRAM_COUNTS("Cast.Stats.AudioFrameCaptured",
                it->second->framerate_fps);
            break;
          case kAudioFrameEncoded:
            UMA_HISTOGRAM_COUNTS("Cast.Stats.AudioFrameEncoded",
                it->second->framerate_fps);
            break;
          case kVideoFrameCaptured:
            UMA_HISTOGRAM_COUNTS("Cast.Stats.VideoFrameCaptured",
                it->second->framerate_fps);
            break;
          case kVideoFrameReceived:
            UMA_HISTOGRAM_COUNTS("Cast.Stats.VideoFrameReceived",
                it->second->framerate_fps);
            break;
          case kVideoFrameSentToEncoder:
            UMA_HISTOGRAM_COUNTS("Cast.Stats.VideoFrameSentToEncoder",
                it->second->framerate_fps);
            break;
          case kVideoFrameEncoded:
            UMA_HISTOGRAM_COUNTS("Cast.Stats.VideoFrameEncoded",
                it->second->framerate_fps);
            break;
          case kVideoFrameDecoded:
            UMA_HISTOGRAM_COUNTS("Cast.Stats.VideoFrameDecoded",
                it->second->framerate_fps);
            break;
          default:
            // No-op
            break;
        }
      } else {
        // All active frame events trigger frame rate computation.
        continue;
      }
      // Bit rate should only be provided following encoding for either audio
      // or video.
      if (it->first == kVideoFrameEncoded) {
        UMA_HISTOGRAM_COUNTS("Cast.Stats.VideoBitrateKbps",
            it->second->framerate_fps);
      } else if (it->first == kAudioFrameEncoded) {
        UMA_HISTOGRAM_COUNTS("Cast.Stats.AudioBitrateKbps",
            it->second->framerate_fps);
      }
      // Delay events.
      if (it->first == kAudioPlayoutDelay) {
        UMA_HISTOGRAM_COUNTS("Cast.Stats.AudioPlayoutDelayAvg",
            it->second->avg_delay_ms);
        UMA_HISTOGRAM_COUNTS("Cast.Stats.AudioPlayoutDelayMin",
            it->second->min_delay_ms);
        UMA_HISTOGRAM_COUNTS("Cast.Stats.AudioPlayoutDelayMax",
            it->second->max_delay_ms);
      } else if (it->first == kVideoRenderDelay) {
        UMA_HISTOGRAM_COUNTS("Cast.Stats.VideoPlayoutDelayAvg",
            it->second->avg_delay_ms);
        UMA_HISTOGRAM_COUNTS("Cast.Stats.VideoPlayoutDelayMin",
            it->second->min_delay_ms);
        UMA_HISTOGRAM_COUNTS("Cast.Stats.VideoPlayoutDelayMax",
            it->second->max_delay_ms);
      }
    }
  }
  return stats;
}

const PacketStatsMap* LoggingImpl::GetPacketStatsData(
    const base::TimeTicks& now) {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  // Get stats data.
  const PacketStatsMap* stats = stats_.GetPacketStatsData(now);
  if (config_.enable_uma_stats) {
    PacketStatsMap::const_iterator it;
    for (it = stats->begin(); it != stats->end(); ++it) {
      switch (it->first) {
        case kPacketSentToPacer:
          UMA_HISTOGRAM_COUNTS("Cast.Stats.PacketSentToPacer", it->second);
          break;
        case kPacketSentToNetwork:
          UMA_HISTOGRAM_COUNTS("Cast.Stats.PacketSentToNetwork", it->second);
          break;
        case kPacketRetransmitted:
          UMA_HISTOGRAM_COUNTS("Cast.Stats.PacketRetransmited", it->second);
          break;
        case kDuplicatePacketReceived:
          UMA_HISTOGRAM_COUNTS("Cast.Stats.DuplicatePacketReceived",
                               it->second);
          break;
        default:
          // No-op.
          break;
      }
    }
  }
  return stats;
}

const GenericStatsMap* LoggingImpl::GetGenericStatsData() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  // Get stats data.
  const GenericStatsMap* stats = stats_.GetGenericStatsData();
  return stats;
}

void LoggingImpl::ResetRaw() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  raw_.Reset();
}

void LoggingImpl::ResetStats() {
  DCHECK(main_thread_proxy_->RunsTasksOnCurrentThread());
  stats_.Reset();
}

}  // namespace cast
}  // namespace media
