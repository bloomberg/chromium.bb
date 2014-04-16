// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/encoding_event_subscriber.h"

#include <cstring>
#include <utility>

#include "base/logging.h"
#include "media/cast/logging/proto/proto_utils.h"

using google::protobuf::RepeatedPtrField;
using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::BasePacketEvent;
using media::cast::proto::LogMetadata;

namespace media {
namespace cast {

EncodingEventSubscriber::EncodingEventSubscriber(
    EventMediaType event_media_type,
    size_t max_frames)
    : event_media_type_(event_media_type),
      max_frames_(max_frames),
      seen_first_rtp_timestamp_(false),
      first_rtp_timestamp_(0u) {}

EncodingEventSubscriber::~EncodingEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void EncodingEventSubscriber::OnReceiveFrameEvent(
    const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ShouldProcessEvent(frame_event.type)) {
    RtpTimestamp relative_rtp_timestamp =
        GetRelativeRtpTimestamp(frame_event.rtp_timestamp);
    FrameEventMap::iterator it = frame_event_map_.find(relative_rtp_timestamp);
    linked_ptr<AggregatedFrameEvent> event_proto;

    // Look up existing entry. If not found, create a new entry and add to map.
    if (it == frame_event_map_.end()) {
      event_proto.reset(new AggregatedFrameEvent);
      event_proto->set_relative_rtp_timestamp(relative_rtp_timestamp);
      frame_event_map_.insert(
          std::make_pair(relative_rtp_timestamp, event_proto));
    } else {
      event_proto = it->second;
    }

    event_proto->add_event_type(ToProtoEventType(frame_event.type));
    event_proto->add_event_timestamp_ms(
        (frame_event.timestamp - base::TimeTicks()).InMilliseconds());

    if (frame_event.type == kAudioFrameEncoded) {
      event_proto->set_encoded_frame_size(frame_event.size);
    } else if (frame_event.type == kVideoFrameEncoded) {
      event_proto->set_encoded_frame_size(frame_event.size);
      event_proto->set_key_frame(frame_event.key_frame);
    } else if (frame_event.type == kAudioPlayoutDelay ||
               frame_event.type == kVideoRenderDelay) {
      event_proto->set_delay_millis(frame_event.delay_delta.InMilliseconds());
    }

    TruncateFrameEventMapIfNeeded();
  }

  DCHECK(frame_event_map_.size() <= max_frames_);
}

void EncodingEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ShouldProcessEvent(packet_event.type)) {
    RtpTimestamp relative_rtp_timestamp =
        GetRelativeRtpTimestamp(packet_event.rtp_timestamp);
    PacketEventMap::iterator it =
        packet_event_map_.find(relative_rtp_timestamp);
    linked_ptr<AggregatedPacketEvent> event_proto;
    BasePacketEvent* base_packet_event_proto = NULL;

    // Look up existing entry. If not found, create a new entry and add to map.
    if (it == packet_event_map_.end()) {
      event_proto.reset(new AggregatedPacketEvent);
      event_proto->set_relative_rtp_timestamp(relative_rtp_timestamp);
      packet_event_map_.insert(
          std::make_pair(relative_rtp_timestamp, event_proto));
      base_packet_event_proto = event_proto->add_base_packet_event();
      base_packet_event_proto->set_packet_id(packet_event.packet_id);
    } else {
      // Found existing entry, now look up existing BasePacketEvent using packet
      // ID. If not found, create a new entry and add to proto.
      event_proto = it->second;
      RepeatedPtrField<BasePacketEvent>* field =
          event_proto->mutable_base_packet_event();
      for (RepeatedPtrField<BasePacketEvent>::pointer_iterator it =
               field->pointer_begin();
           it != field->pointer_end();
           ++it) {
        if ((*it)->packet_id() == packet_event.packet_id) {
          base_packet_event_proto = *it;
          break;
        }
      }
      if (!base_packet_event_proto) {
        base_packet_event_proto = event_proto->add_base_packet_event();
        base_packet_event_proto->set_packet_id(packet_event.packet_id);
      }
    }

    base_packet_event_proto->add_event_type(
        ToProtoEventType(packet_event.type));
    base_packet_event_proto->add_event_timestamp_ms(
        (packet_event.timestamp - base::TimeTicks()).InMilliseconds());

    TruncatePacketEventMapIfNeeded();
  }

  DCHECK(packet_event_map_.size() <= max_frames_);
}

void EncodingEventSubscriber::OnReceiveGenericEvent(
    const GenericEvent& generic_event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Do nothing, there are no generic events we are interested in.
}

void EncodingEventSubscriber::GetEventsAndReset(LogMetadata* metadata,
                                                FrameEventMap* frame_events,
                                                PacketEventMap* packet_events) {
  DCHECK(thread_checker_.CalledOnValidThread());

  metadata->set_is_audio(event_media_type_ == AUDIO_EVENT);
  metadata->set_first_rtp_timestamp(first_rtp_timestamp_);
  metadata->set_num_frame_events(frame_event_map_.size());
  metadata->set_num_packet_events(packet_event_map_.size());
  metadata->set_reference_timestamp_ms_at_unix_epoch(
      (base::TimeTicks::UnixEpoch() - base::TimeTicks()).InMilliseconds());
  frame_events->swap(frame_event_map_);
  packet_events->swap(packet_event_map_);
  Reset();
}

bool EncodingEventSubscriber::ShouldProcessEvent(CastLoggingEvent event) {
  return GetEventMediaType(event) == event_media_type_;
}

void EncodingEventSubscriber::TruncateFrameEventMapIfNeeded() {
  // This works because this is called everytime an event is inserted and
  // we only insert events one at a time.
  if (frame_event_map_.size() > max_frames_)
    frame_event_map_.erase(frame_event_map_.begin());
}

void EncodingEventSubscriber::TruncatePacketEventMapIfNeeded() {
  // This works because this is called everytime an event is inserted and
  // we only insert events one at a time.
  if (packet_event_map_.size() > max_frames_)
    packet_event_map_.erase(packet_event_map_.begin());
}

RtpTimestamp EncodingEventSubscriber::GetRelativeRtpTimestamp(
    RtpTimestamp rtp_timestamp) {
  if (!seen_first_rtp_timestamp_) {
    seen_first_rtp_timestamp_ = true;
    first_rtp_timestamp_ = rtp_timestamp;
  }

  return rtp_timestamp - first_rtp_timestamp_;
}

void EncodingEventSubscriber::Reset() {
  frame_event_map_.clear();
  packet_event_map_.clear();
  seen_first_rtp_timestamp_ = false;
  first_rtp_timestamp_ = 0u;
}

}  // namespace cast
}  // namespace media
