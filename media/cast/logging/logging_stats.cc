// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "media/cast/logging/logging_stats.h"

namespace media {
namespace cast {

LoggingStats::LoggingStats(base::TickClock* clock)
    : frame_stats_(),
      packet_stats_(),
      generic_stats_(),
      start_time_(),
      clock_(clock) {
  memset(counts_, 0, sizeof(counts_));
  memset(start_time_, 0, sizeof(start_time_));
}

LoggingStats::~LoggingStats() {}

void LoggingStats::Reset() {
  frame_stats_.clear();
  packet_stats_.clear();
  generic_stats_.clear();
  memset(counts_, 0, sizeof(counts_));
}

void LoggingStats::InsertFrameEvent(CastLoggingEvent event,
                                    uint32 rtp_timestamp,
                                    uint32 frame_id) {
  InsertBaseFrameEvent(event, frame_id, rtp_timestamp);
}

void LoggingStats::InsertFrameEventWithSize(CastLoggingEvent event,
                                            uint32 rtp_timestamp,
                                            uint32 frame_id,
                                            int frame_size) {
  InsertBaseFrameEvent(event, frame_id, rtp_timestamp);
  // Update size.
  FrameStatsMap::iterator it = frame_stats_.find(event);
  DCHECK(it != frame_stats_.end());
  it->second->bitrate_kbps += frame_size;
}

void LoggingStats::InsertFrameEventWithDelay(CastLoggingEvent event,
                                             uint32 rtp_timestamp,
                                             uint32 frame_id,
                                             base::TimeDelta delay) {
  InsertBaseFrameEvent(event, frame_id, rtp_timestamp);
  // Update size.
  FrameStatsMap::iterator it = frame_stats_.find(event);
  DCHECK(it != frame_stats_.end());
  // Using the average delay as a counter, will divide by the counter when
  // triggered.
  it->second->avg_delay_ms += delay.InMilliseconds();
  if (delay.InMilliseconds() > it->second->max_delay_ms)
    it->second->max_delay_ms = delay.InMilliseconds();
  if ((delay.InMilliseconds() < it->second->min_delay_ms) ||
      (counts_[event] == 1) )
    it->second->min_delay_ms = delay.InMilliseconds();
}

void LoggingStats::InsertBaseFrameEvent(CastLoggingEvent event,
                                        uint32 frame_id,
                                        uint32 rtp_timestamp) {
  // Does this belong to an existing event?
  FrameStatsMap::iterator it = frame_stats_.find(event);
  if (it == frame_stats_.end()) {
    // New event.
    start_time_[event] = clock_->NowTicks();
    linked_ptr<FrameLogStats> stats(new FrameLogStats());
    frame_stats_.insert(std::make_pair(event, stats));
  }

  ++counts_[event];
}

void LoggingStats::InsertPacketEvent(CastLoggingEvent event,
                                     uint32 rtp_timestamp,
                                     uint32 frame_id,
                                     uint16 packet_id,
                                     uint16 max_packet_id,
                                     size_t size) {
  // Does this packet belong to an existing event?
  PacketStatsMap::iterator it = packet_stats_.find(event);
  if (it == packet_stats_.end()) {
    // New event.
    start_time_[event] = clock_->NowTicks();
    packet_stats_.insert(std::make_pair(event, size));
  } else {
    // Add to existing.
    it->second += size;
  }
  ++counts_[event];
}

void LoggingStats::InsertGenericEvent(CastLoggingEvent event, int value) {
  // Does this event belong to an existing event?
  GenericStatsMap::iterator it = generic_stats_.find(event);
  if (it == generic_stats_.end()) {
    // New event.
    start_time_[event] = clock_->NowTicks();
    generic_stats_.insert(std::make_pair(event, value));
  } else {
    // Add to existing (will be used to compute average).
    it->second += value;
  }
   ++counts_[event];
}

const FrameStatsMap* LoggingStats::GetFrameStatsData() {
  // Compute framerate and bitrate (when available).
  FrameStatsMap::iterator it;
  for (it = frame_stats_.begin(); it != frame_stats_.end(); ++it) {
    base::TimeDelta time_diff = clock_->NowTicks() - start_time_[it->first];
    it->second->framerate_fps = counts_[it->first] / time_diff.InSecondsF();
    if (it->second->bitrate_kbps > 0) {
      it->second->bitrate_kbps = (8 / 1000) *
          it->second->bitrate_kbps / time_diff.InSecondsF();
    }
    if (it->second->avg_delay_ms > 0)
      it->second->avg_delay_ms /= counts_[it->first];
  }
  return &frame_stats_;
}

const PacketStatsMap* LoggingStats::GetPacketStatsData() {
  PacketStatsMap::iterator it;
  for (it = packet_stats_.begin(); it != packet_stats_.end(); ++it) {
    if (counts_[it->first] == 0) continue;
    base::TimeDelta time_diff = clock_->NowTicks() - start_time_[it->first];
    it->second = (8 / 1000) * it->second / time_diff.InSecondsF();
  }
  return &packet_stats_;
}

const GenericStatsMap* LoggingStats::GetGenericStatsData() {
  // Compute averages.
  GenericStatsMap::iterator it;
  for (it = generic_stats_.begin(); it != generic_stats_.end(); ++it) {
    it->second /= counts_[ it->first];
  }
  return &generic_stats_;
}

}  // namespace cast
}  // namespace media
