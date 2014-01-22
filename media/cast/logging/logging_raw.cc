// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging_raw.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"

namespace media {
namespace cast {

LoggingRaw::LoggingRaw(bool is_sender)
    : is_sender_(is_sender),
      frame_map_(),
      packet_map_(),
      generic_map_(),
      weak_factory_(this) {}

LoggingRaw::~LoggingRaw() {}

void LoggingRaw::InsertFrameEvent(const base::TimeTicks& time_of_event,
                                  CastLoggingEvent event,
                                  uint32 rtp_timestamp,
                                  uint32 frame_id) {
  InsertBaseFrameEvent(time_of_event, event, frame_id, rtp_timestamp);

  InsertRtcpFrameEvent(time_of_event, event, rtp_timestamp, base::TimeDelta());
}

void LoggingRaw::InsertFrameEventWithSize(const base::TimeTicks& time_of_event,
                                          CastLoggingEvent event,
                                          uint32 rtp_timestamp,
                                          uint32 frame_id,
                                          int size) {
  InsertBaseFrameEvent(time_of_event, event, frame_id, rtp_timestamp);
  // Now insert size.
  FrameRawMap::iterator it = frame_map_.find(rtp_timestamp);
  DCHECK(it != frame_map_.end());
  it->second.size = size;
}

void LoggingRaw::InsertFrameEventWithDelay(const base::TimeTicks& time_of_event,
                                           CastLoggingEvent event,
                                           uint32 rtp_timestamp,
                                           uint32 frame_id,
                                           base::TimeDelta delay) {
  InsertBaseFrameEvent(time_of_event, event, frame_id, rtp_timestamp);
  // Now insert delay.
  FrameRawMap::iterator it = frame_map_.find(rtp_timestamp);
  DCHECK(it != frame_map_.end());
  it->second.delay_delta = delay;

  InsertRtcpFrameEvent(time_of_event, event, rtp_timestamp, delay);
}

void LoggingRaw::InsertBaseFrameEvent(const base::TimeTicks& time_of_event,
                                      CastLoggingEvent event,
                                      uint32 frame_id,
                                      uint32 rtp_timestamp) {
  // Is this a new event?
  FrameRawMap::iterator it = frame_map_.find(rtp_timestamp);
  if (it == frame_map_.end()) {
    // Create a new map entry.
    FrameEvent info;
    info.frame_id = frame_id;
    info.timestamp.push_back(time_of_event);
    info.type.push_back(event);
    frame_map_.insert(std::make_pair(rtp_timestamp, info));
  } else {
    // Insert to an existing entry.
    it->second.timestamp.push_back(time_of_event);
    it->second.type.push_back(event);
    // Do we have a valid frame_id?
    // Not all events have a valid frame id.
    if (it->second.frame_id == kFrameIdUnknown && frame_id != kFrameIdUnknown)
      it->second.frame_id = frame_id;
  }
}

void LoggingRaw::InsertPacketEvent(const base::TimeTicks& time_of_event,
                                   CastLoggingEvent event,
                                   uint32 rtp_timestamp,
                                   uint32 frame_id,
                                   uint16 packet_id,
                                   uint16 max_packet_id,
                                   size_t size) {
  // Is this packet belonging to a new frame?
  PacketRawMap::iterator it = packet_map_.find(rtp_timestamp);
  if (it == packet_map_.end()) {
    // Create a new entry - start with base packet map.
    PacketEvent info;
    info.frame_id = frame_id;
    info.max_packet_id = max_packet_id;
    BasePacketInfo base_info;
    base_info.size = size;
    base_info.timestamp.push_back(time_of_event);
    base_info.type.push_back(event);
    info.packet_map.insert(std::make_pair(packet_id, base_info));
    packet_map_.insert(std::make_pair(rtp_timestamp, info));
  } else {
    // Is this a new packet?
    BasePacketMap::iterator packet_it = it->second.packet_map.find(packet_id);
    if (packet_it == it->second.packet_map.end()) {
      BasePacketInfo base_info;
      base_info.size = size;
      base_info.timestamp.push_back(time_of_event);
      base_info.type.push_back(event);
      it->second.packet_map.insert(std::make_pair(packet_id, base_info));
    } else {
      packet_it->second.timestamp.push_back(time_of_event);
      packet_it->second.type.push_back(event);
    }
  }
  if (event == kAudioPacketReceived) {
      ReceiverRtcpEvent rtcp_event;
      rtcp_event.type = event;
      rtcp_event.timestamp = time_of_event;
      rtcp_event.packet_id = packet_id;
      audio_rtcp_map_.insert(std::make_pair(rtp_timestamp, rtcp_event));
  } else if (event == kVideoPacketReceived) {
      ReceiverRtcpEvent rtcp_event;
      rtcp_event.type = event;
      rtcp_event.timestamp = time_of_event;
      rtcp_event.packet_id = packet_id;
      video_rtcp_map_.insert(std::make_pair(rtp_timestamp, rtcp_event));
  }
}

void LoggingRaw::InsertGenericEvent(const base::TimeTicks& time_of_event,
                                    CastLoggingEvent event, int value) {
  GenericEvent event_data;
  event_data.value.push_back(value);
  event_data.timestamp.push_back(time_of_event);
  // Is this a new event?
  GenericRawMap::iterator it = generic_map_.find(event);
  if (it == generic_map_.end()) {
    // Create new entry.
    generic_map_.insert(std::make_pair(event, event_data));
  } else {
    // Insert to existing entry.
    it->second.value.push_back(value);
    it->second.timestamp.push_back(time_of_event);
  }
}

void LoggingRaw::InsertRtcpFrameEvent(const base::TimeTicks& time_of_event,
                                      CastLoggingEvent event,
                                      uint32 rtp_timestamp,
                                      base::TimeDelta delay) {
  ReceiverRtcpEvent rtcp_event;
  if (is_sender_) {
    if (event != kVideoFrameCaptured &&
        event != kVideoFrameSentToEncoder &&
        event != kVideoFrameEncoded) {
      // Not interested in other events.
      return;
    }
    VideoRtcpRawMap::iterator it = video_rtcp_map_.find(rtp_timestamp);
    if (it == video_rtcp_map_.end()) {
      // We have not stored this frame (RTP timestamp) in our map.
      rtcp_event.type = event;
      video_rtcp_map_.insert(std::make_pair(rtp_timestamp, rtcp_event));
    } else {
      // We already have this frame (RTP timestamp) in our map.
      // Only update events that are later in the chain.
      // This is due to that events can be reordered on the wire.
      if (event == kVideoFrameCaptured) {
        return;  // First event in chain can not be late by definition.
      }
      if (it->second.type == kVideoFrameEncoded) {
        return;  // Last event in chain should not be updated.
      }
      // Update existing entry.
      it->second.type = event;
    }
  } else {
    // We are a cast receiver.
    switch (event) {
      case kAudioPlayoutDelay:
        rtcp_event.delay_delta = delay;
      case kAudioFrameDecoded:
      case kAudioAckSent:
        rtcp_event.type = event;
        rtcp_event.timestamp = time_of_event;
        audio_rtcp_map_.insert(std::make_pair(rtp_timestamp, rtcp_event));
        break;
      case kVideoRenderDelay:
        rtcp_event.delay_delta = delay;
      case kVideoFrameDecoded:
      case kVideoAckSent:
        rtcp_event.type = event;
        rtcp_event.timestamp = time_of_event;
        video_rtcp_map_.insert(std::make_pair(rtp_timestamp, rtcp_event));
      break;
      default:
        break;
    }
  }
}

FrameRawMap LoggingRaw::GetFrameData() const {
  return frame_map_;
}

PacketRawMap LoggingRaw::GetPacketData() const {
  return packet_map_;
}

GenericRawMap LoggingRaw::GetGenericData() const {
  return generic_map_;
}

AudioRtcpRawMap LoggingRaw::GetAndResetAudioRtcpData() {
  AudioRtcpRawMap return_map;
  audio_rtcp_map_.swap(return_map);
  return return_map;
}

VideoRtcpRawMap LoggingRaw::GetAndResetVideoRtcpData() {
  VideoRtcpRawMap return_map;
  video_rtcp_map_.swap(return_map);
  return return_map;
}

void LoggingRaw::Reset() {
  frame_map_.clear();
  packet_map_.clear();
  generic_map_.clear();
}

}  // namespace cast
}  // namespace media
