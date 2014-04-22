// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_STATS_EVENT_SUBSCRIBER_H_
#define MEDIA_CAST_LOGGING_STATS_EVENT_SUBSCRIBER_H_

#include "base/threading/thread_checker.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/raw_event_subscriber.h"

namespace media {
namespace cast {

// A RawEventSubscriber implementation that subscribes to events,
// and aggregates them into stats.
class StatsEventSubscriber : public RawEventSubscriber {
 public:
  StatsEventSubscriber(EventMediaType media_type);

  virtual ~StatsEventSubscriber();

  // RawReventSubscriber implementations.
  virtual void OnReceiveFrameEvent(const FrameEvent& frame_event) OVERRIDE;
  virtual void OnReceivePacketEvent(const PacketEvent& packet_event) OVERRIDE;

  // Assigns |frame_stats_map| with frame stats.
  void GetFrameStats(FrameStatsMap* frame_stats_map) const;

  // Assigns |packet_stats_map| with packet stats.
  void GetPacketStats(PacketStatsMap* packet_stats_map) const;

  // Resets all stats maps in this object.
  void Reset();

 private:
  EventMediaType event_media_type_;
  FrameStatsMap frame_stats_;
  PacketStatsMap packet_stats_;
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(StatsEventSubscriber);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_STATS_EVENT_SUBSCRIBER_H_
