// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/stats_util.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace media {
namespace cast {

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
    frame_stats->SetInteger("sizeTotal", static_cast<int>(it->second.sum_size));
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
