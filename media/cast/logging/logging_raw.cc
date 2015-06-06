// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging_raw.h"

#include <algorithm>

#include "base/logging.h"
#include "base/time/time.h"

namespace media {
namespace cast {

LoggingRaw::LoggingRaw() {}

LoggingRaw::~LoggingRaw() {}

void LoggingRaw::InsertFrameEvent(const base::TimeTicks& time_of_event,
                                  CastLoggingEvent event,
                                  EventMediaType event_media_type,
                                  uint32 rtp_timestamp,
                                  uint32 frame_id) {
  InsertBaseFrameEvent(time_of_event, event, event_media_type, frame_id,
                       rtp_timestamp, base::TimeDelta(), 0, 0, 0, false, 0,
                       -1.0, -1.0);
}

void LoggingRaw::InsertCapturedVideoFrameEvent(
    const base::TimeTicks& time_of_event,
    uint32 rtp_timestamp,
    int width,
    int height) {
 InsertBaseFrameEvent(time_of_event, FRAME_CAPTURE_END, VIDEO_EVENT,
                       kFrameIdUnknown, rtp_timestamp, base::TimeDelta(), width,
                       height, 0, false, 0, -1.0, -1.0);
}

void LoggingRaw::InsertEncodedFrameEvent(const base::TimeTicks& time_of_event,
                                         CastLoggingEvent event,
                                         EventMediaType event_media_type,
                                         uint32 rtp_timestamp,
                                         uint32 frame_id,
                                         int encoded_size,
                                         bool key_frame,
                                         int target_bitrate,
                                         double encoder_cpu_utilization,
                                         double idealized_bitrate_utilization) {
  InsertBaseFrameEvent(time_of_event, event, event_media_type,
                       frame_id, rtp_timestamp, base::TimeDelta(),
                       0, 0, encoded_size, key_frame, target_bitrate,
                       encoder_cpu_utilization, idealized_bitrate_utilization);
}

void LoggingRaw::InsertFrameEventWithDelay(const base::TimeTicks& time_of_event,
                                           CastLoggingEvent event,
                                           EventMediaType event_media_type,
                                           uint32 rtp_timestamp,
                                           uint32 frame_id,
                                           base::TimeDelta delay) {
  InsertBaseFrameEvent(time_of_event, event, event_media_type, frame_id,
                       rtp_timestamp, delay, 0, 0, 0, false, 0, -1.0, -1.0);
}

void LoggingRaw::InsertBaseFrameEvent(const base::TimeTicks& time_of_event,
                                      CastLoggingEvent event,
                                      EventMediaType event_media_type,
                                      uint32 frame_id,
                                      uint32 rtp_timestamp,
                                      base::TimeDelta delay,
                                      int width,
                                      int height,
                                      int encoded_size,
                                      bool key_frame,
                                      int target_bitrate,
                                      double encoder_cpu_utilization,
                                      double idealized_bitrate_utilization) {
  FrameEvent frame_event;
  frame_event.rtp_timestamp = rtp_timestamp;
  frame_event.frame_id = frame_id;
  frame_event.width = width;
  frame_event.height = height;
  frame_event.size = encoded_size;
  frame_event.timestamp = time_of_event;
  frame_event.type = event;
  frame_event.media_type = event_media_type;
  frame_event.delay_delta = delay;
  frame_event.key_frame = key_frame;
  frame_event.target_bitrate = target_bitrate;
  frame_event.encoder_cpu_utilization = encoder_cpu_utilization;
  frame_event.idealized_bitrate_utilization = idealized_bitrate_utilization;
  for (std::vector<RawEventSubscriber*>::const_iterator it =
           subscribers_.begin();
       it != subscribers_.end(); ++it) {
    (*it)->OnReceiveFrameEvent(frame_event);
  }
}

void LoggingRaw::InsertPacketEvent(const base::TimeTicks& time_of_event,
                                   CastLoggingEvent event,
                                   EventMediaType event_media_type,
                                   uint32 rtp_timestamp,
                                   uint32 frame_id, uint16 packet_id,
                                   uint16 max_packet_id, size_t size) {
  PacketEvent packet_event;
  packet_event.rtp_timestamp = rtp_timestamp;
  packet_event.frame_id = frame_id;
  packet_event.max_packet_id = max_packet_id;
  packet_event.packet_id = packet_id;
  packet_event.size = size;
  packet_event.timestamp = time_of_event;
  packet_event.type = event;
  packet_event.media_type = event_media_type;
  for (std::vector<RawEventSubscriber*>::const_iterator it =
           subscribers_.begin();
       it != subscribers_.end(); ++it) {
    (*it)->OnReceivePacketEvent(packet_event);
  }
}

void LoggingRaw::AddSubscriber(RawEventSubscriber* subscriber) {
  DCHECK(subscriber);
  DCHECK(std::find(subscribers_.begin(), subscribers_.end(), subscriber) ==
      subscribers_.end());

  subscribers_.push_back(subscriber);
}

void LoggingRaw::RemoveSubscriber(RawEventSubscriber* subscriber) {
  DCHECK(subscriber);
  DCHECK(std::find(subscribers_.begin(), subscribers_.end(), subscriber) !=
      subscribers_.end());

  subscribers_.erase(
      std::remove(subscribers_.begin(), subscribers_.end(), subscriber),
      subscribers_.end());
}

}  // namespace cast
}  // namespace media
