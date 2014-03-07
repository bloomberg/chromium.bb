// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging_stats.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace media {
namespace cast {

LoggingStats::LoggingStats()
    : frame_stats_(),
      packet_stats_(),
      generic_stats_() {
}

LoggingStats::~LoggingStats() {}

void LoggingStats::Reset() {
  frame_stats_.clear();
  packet_stats_.clear();
  generic_stats_.clear();
}

void LoggingStats::InsertFrameEvent(const base::TimeTicks& time_of_event,
                                    CastLoggingEvent event,
                                    uint32 rtp_timestamp,
                                    uint32 frame_id) {
  InsertBaseFrameEvent(time_of_event, event, frame_id, rtp_timestamp);
}

void LoggingStats::InsertFrameEventWithSize(
    const base::TimeTicks& time_of_event,
    CastLoggingEvent event,
    uint32 rtp_timestamp,
    uint32 frame_id,
    int frame_size) {
  InsertBaseFrameEvent(time_of_event, event, frame_id, rtp_timestamp);
  // Update size.
  FrameStatsMap::iterator it = frame_stats_.find(event);
  DCHECK(it != frame_stats_.end());
  it->second.sum_size += frame_size;
}

void LoggingStats::InsertFrameEventWithDelay(
    const base::TimeTicks& time_of_event,
    CastLoggingEvent event,
    uint32 rtp_timestamp,
    uint32 frame_id,
    base::TimeDelta delay) {
  InsertBaseFrameEvent(time_of_event, event, frame_id, rtp_timestamp);
  // Update size.
  FrameStatsMap::iterator it = frame_stats_.find(event);
  DCHECK(it != frame_stats_.end());
  it->second.sum_delay += delay;
  if (delay > it->second.max_delay || it->second.event_counter == 1)
    it->second.max_delay = delay;
  if (delay < it->second.min_delay || it->second.event_counter == 1)
    it->second.min_delay = delay;
}

void LoggingStats::InsertBaseFrameEvent(const base::TimeTicks& time_of_event,
                                        CastLoggingEvent event,
                                        uint32 frame_id,
                                        uint32 rtp_timestamp) {
  // Does this belong to an existing event?
  FrameStatsMap::iterator it = frame_stats_.find(event);
  if (it == frame_stats_.end()) {
    // New event.
    FrameLogStats stats;
    stats.first_event_time = time_of_event;
    stats.last_event_time = time_of_event;
    stats.event_counter = 1;
    frame_stats_.insert(std::make_pair(event, stats));
  } else {
    ++(it->second.event_counter);
    it->second.last_event_time = time_of_event;
  }
}

void LoggingStats::InsertPacketEvent(const base::TimeTicks& time_of_event,
                                     CastLoggingEvent event,
                                     uint32 rtp_timestamp,
                                     uint32 frame_id,
                                     uint16 packet_id,
                                     uint16 max_packet_id,
                                     size_t size) {
  // Does this packet belong to an existing event?
  PacketStatsMap::iterator it = packet_stats_.find(event);
  if (it == packet_stats_.end()) {
    // New event.
    PacketLogStats stats;
    stats.first_event_time = time_of_event;
    stats.last_event_time = time_of_event;
    stats.sum_size = size;
    stats.event_counter = 1;
    packet_stats_.insert(std::make_pair(event, stats));
  } else {
    // Add to an existing event.
    it->second.sum_size += size;
    ++(it->second.event_counter);
    it->second.last_event_time = time_of_event;
  }
}

void LoggingStats::InsertGenericEvent(const base::TimeTicks& time_of_event,
                                      CastLoggingEvent event, int value) {
  // Does this event belong to an existing event?
  GenericStatsMap::iterator it = generic_stats_.find(event);
  if (it == generic_stats_.end()) {
    // New event.
    GenericLogStats stats;
    stats.first_event_time = time_of_event;
    stats.last_event_time = time_of_event;
    stats.sum = value;
    stats.sum_squared = value * value;
    stats.min = value;
    stats.max = value;
    stats.event_counter = 1;
    generic_stats_.insert(std::make_pair(event, stats));
  } else {
    // Add to existing event.
    it->second.sum += value;
    it->second.sum_squared += value * value;
    ++(it->second.event_counter);
    it->second.last_event_time = time_of_event;
    if (it->second.min > value) {
      it->second.min = value;
    } else if (it->second.max < value) {
      it->second.max = value;
    }
  }
}

FrameStatsMap LoggingStats::GetFrameStatsData(EventMediaType media_type) const {
  DCHECK(media_type == AUDIO_EVENT || media_type == VIDEO_EVENT);

  FrameStatsMap frame_map_to_return;
  for (FrameStatsMap::const_iterator it = frame_stats_.begin();
       it != frame_stats_.end();
       ++it) {
    if (GetEventMediaType(it->first) == media_type) {
      frame_map_to_return.insert(std::make_pair(it->first, it->second));
    }
  }

  return frame_map_to_return;
}

PacketStatsMap LoggingStats::GetPacketStatsData(
    EventMediaType media_type) const {
  DCHECK(media_type == AUDIO_EVENT || media_type == VIDEO_EVENT);

  PacketStatsMap packet_map_to_return;
  for (PacketStatsMap::const_iterator it = packet_stats_.begin();
       it != packet_stats_.end();
       ++it) {
    if (GetEventMediaType(it->first) == media_type) {
      packet_map_to_return.insert(std::make_pair(it->first, it->second));
    }
  }

  return packet_map_to_return;
}

GenericStatsMap LoggingStats::GetGenericStatsData() const {
  return generic_stats_;
}

scoped_ptr<base::DictionaryValue> ConvertStats(
    const FrameStatsMap& frame_stats_map,
    const PacketStatsMap& packet_stats_map) {
  scoped_ptr<base::DictionaryValue> overall_stats(new base::DictionaryValue);

  scoped_ptr<base::DictionaryValue> overall_frame_stats(
      new base::DictionaryValue);
  for (FrameStatsMap::const_iterator it = frame_stats_map.begin();
       it != frame_stats_map.end();
       ++it) {
    scoped_ptr<base::DictionaryValue> frame_stats(new base::DictionaryValue);

    frame_stats->SetDouble("firstEventTime",
                           it->second.first_event_time.ToInternalValue());
    frame_stats->SetDouble("lastEventTime",
                           it->second.last_event_time.ToInternalValue());
    frame_stats->SetInteger("count", it->second.event_counter);
    frame_stats->SetInteger("sizeTotal",
                            static_cast<int>(it->second.sum_size));
    frame_stats->SetInteger("minDelayMs",
                            it->second.min_delay.InMilliseconds());
    frame_stats->SetInteger("maxDelayMs",
                            it->second.max_delay.InMilliseconds());
    frame_stats->SetInteger("sumDelayMs",
                            it->second.sum_delay.InMilliseconds());

    overall_frame_stats->Set(CastLoggingToString(it->first),
                             frame_stats.release());
  }

  overall_stats->Set("frameStats", overall_frame_stats.release());

  scoped_ptr<base::DictionaryValue> overall_packet_stats(
      new base::DictionaryValue);
  for (PacketStatsMap::const_iterator it = packet_stats_map.begin();
       it != packet_stats_map.end();
       ++it) {
    scoped_ptr<base::DictionaryValue> packet_stats(new base::DictionaryValue);

    packet_stats->SetDouble("firstEventTime",
                            it->second.first_event_time.ToInternalValue());
    packet_stats->SetDouble("lastEventTime",
                            it->second.last_event_time.ToInternalValue());
    packet_stats->SetDouble("lastEventTime",
                            it->second.last_event_time.ToInternalValue());
    packet_stats->SetInteger("count", it->second.event_counter);
    packet_stats->SetInteger("sizeTotal",
                             static_cast<int>(it->second.sum_size));

    overall_packet_stats->Set(CastLoggingToString(it->first),
                              packet_stats.release());
  }

  overall_stats->Set("packetStats", overall_packet_stats.release());

  return overall_stats.Pass();
}

}  // namespace cast
}  // namespace media
