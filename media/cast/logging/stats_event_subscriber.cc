// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/stats_event_subscriber.h"

#include "base/logging.h"

namespace media {
namespace cast {

StatsEventSubscriber::StatsEventSubscriber(EventMediaType event_media_type)
    : event_media_type_(event_media_type) {}

StatsEventSubscriber::~StatsEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void StatsEventSubscriber::OnReceiveFrameEvent(const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CastLoggingEvent type = frame_event.type;
  if (GetEventMediaType(type) != event_media_type_)
    return;

  FrameStatsMap::iterator it = frame_stats_.find(type);
  if (it == frame_stats_.end()) {
    FrameLogStats stats;
    stats.first_event_time = frame_event.timestamp;
    stats.last_event_time = frame_event.timestamp;
    stats.event_counter = 1;
    stats.sum_size = frame_event.size;
    stats.min_delay = frame_event.delay_delta;
    stats.max_delay = frame_event.delay_delta;
    stats.sum_delay = frame_event.delay_delta;
    frame_stats_.insert(std::make_pair(type, stats));
  } else {
    ++(it->second.event_counter);
    it->second.last_event_time = frame_event.timestamp;
    it->second.sum_size += frame_event.size;
    it->second.sum_delay += frame_event.delay_delta;
    if (frame_event.delay_delta > it->second.max_delay)
      it->second.max_delay = frame_event.delay_delta;
    if (frame_event.delay_delta < it->second.min_delay)
      it->second.min_delay = frame_event.delay_delta;
  }
}

void StatsEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CastLoggingEvent type = packet_event.type;
  if (GetEventMediaType(type) != event_media_type_)
    return;

  PacketStatsMap::iterator it = packet_stats_.find(type);
  if (it == packet_stats_.end()) {
    PacketLogStats stats;
    stats.first_event_time = packet_event.timestamp;
    stats.last_event_time = packet_event.timestamp;
    stats.event_counter = 1;
    stats.sum_size = packet_event.size;
    packet_stats_.insert(std::make_pair(type, stats));
  } else {
    it->second.last_event_time = packet_event.timestamp;
    ++(it->second.event_counter);
    it->second.sum_size += packet_event.size;
  }
}

void StatsEventSubscriber::GetFrameStats(FrameStatsMap* frame_stats_map) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(frame_stats_map);

  frame_stats_map->clear();
  frame_stats_map->insert(frame_stats_.begin(), frame_stats_.end());
}

void StatsEventSubscriber::GetPacketStats(
    PacketStatsMap* packet_stats_map) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(packet_stats_map);

  packet_stats_map->clear();
  packet_stats_map->insert(packet_stats_.begin(), packet_stats_.end());
}

void StatsEventSubscriber::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());

  frame_stats_.clear();
  packet_stats_.clear();
}

}  // namespace cast
}  // namespace media
