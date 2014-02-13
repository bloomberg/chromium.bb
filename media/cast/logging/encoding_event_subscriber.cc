// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/encoding_event_subscriber.h"

#include <utility>

#include "base/logging.h"
#include "media/cast/logging/proto/proto_utils.h"

using google::protobuf::RepeatedPtrField;
using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedGenericEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::BasePacketEvent;

namespace media {
namespace cast {

EncodingEventSubscriber::EncodingEventSubscriber() {}

EncodingEventSubscriber::~EncodingEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void EncodingEventSubscriber::OnReceiveFrameEvent(
    const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  FrameEventMap::iterator it = frame_event_map_.find(frame_event.rtp_timestamp);
  linked_ptr<AggregatedFrameEvent> event_proto;

  // Look up existing entry. If not found, create a new entry and add to map.
  if (it == frame_event_map_.end()) {
    event_proto.reset(new AggregatedFrameEvent);
    event_proto->set_rtp_timestamp(frame_event.rtp_timestamp);
    frame_event_map_.insert(
        std::make_pair(frame_event.rtp_timestamp, event_proto));
  } else {
    event_proto = it->second;
  }

  event_proto->add_event_type(ToProtoEventType(frame_event.type));
  event_proto->add_event_timestamp_micros(
      frame_event.timestamp.ToInternalValue());

  if (frame_event.type == kAudioFrameEncoded ||
      frame_event.type == kVideoFrameEncoded) {
    event_proto->set_encoded_frame_size(frame_event.size);
  } else if (frame_event.type == kAudioPlayoutDelay ||
             frame_event.type == kVideoRenderDelay) {
    event_proto->set_delay_millis(frame_event.delay_delta.InMilliseconds());
  }
}

void EncodingEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  PacketEventMap::iterator it =
      packet_event_map_.find(packet_event.rtp_timestamp);
  linked_ptr<AggregatedPacketEvent> event_proto;
  BasePacketEvent* base_packet_event_proto = NULL;

  // Look up existing entry. If not found, create a new entry and add to map.
  if (it == packet_event_map_.end()) {
    event_proto.reset(new AggregatedPacketEvent);
    event_proto->set_rtp_timestamp(packet_event.rtp_timestamp);
    packet_event_map_.insert(
        std::make_pair(packet_event.rtp_timestamp, event_proto));
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
         it != field->pointer_end(); ++it) {
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

  base_packet_event_proto->add_event_type(ToProtoEventType(packet_event.type));
  base_packet_event_proto->add_event_timestamp_micros(
      packet_event.timestamp.ToInternalValue());
}

void EncodingEventSubscriber::OnReceiveGenericEvent(
    const GenericEvent& generic_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  GenericEventMap::iterator it = generic_event_map_.find(generic_event.type);
  linked_ptr<AggregatedGenericEvent> event_proto;
  if (it == generic_event_map_.end()) {
    event_proto.reset(new AggregatedGenericEvent);
    event_proto->set_event_type(ToProtoEventType(generic_event.type));
    generic_event_map_.insert(std::make_pair(generic_event.type, event_proto));
  } else {
    event_proto = it->second;
  }

  event_proto->add_event_timestamp_micros(
      generic_event.timestamp.ToInternalValue());
  event_proto->add_value(generic_event.value);
}

void EncodingEventSubscriber::GetFrameEventsAndReset(
    FrameEventMap* frame_event_map) {
  DCHECK(thread_checker_.CalledOnValidThread());
  frame_event_map->swap(frame_event_map_);
  frame_event_map_.clear();
}

void EncodingEventSubscriber::GetPacketEventsAndReset(
    PacketEventMap* packet_event_map) {
  DCHECK(thread_checker_.CalledOnValidThread());
  packet_event_map->swap(packet_event_map_);
  packet_event_map_.clear();
}

void EncodingEventSubscriber::GetGenericEventsAndReset(
    GenericEventMap* generic_event_map) {
  DCHECK(thread_checker_.CalledOnValidThread());
  generic_event_map->swap(generic_event_map_);
  generic_event_map_.clear();
}

}  // namespace cast
}  // namespace media
