// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOGGING_H_
#define MEDIA_CAST_LOGGING_LOGGING_H_

// Generic class that handles event logging for the cast library.
// Logging has three possible forms:
// 1. [default] Raw data accessible by the application.
// 2. [optional] UMA stats.
// 3. [optional] Tracing.

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/logging_internal.h"

namespace media {
namespace cast {

// Store all log types in a map based on the event.
typedef std::map<CastLoggingEvent, linked_ptr<FrameLogData> > FrameLogMap;
typedef std::map<CastLoggingEvent, linked_ptr<PacketLogData> > PacketLogMap;
typedef std::map<CastLoggingEvent, linked_ptr<GenericLogData> > GenericLogMap;


// This class is not thread safe, and should only be called from the main
// thread.
class Logging : public base::NonThreadSafe,
                public base::SupportsWeakPtr<Logging> {
 public:
  // When tracing is enabled - all events will be added to the trace.
  explicit Logging(base::TickClock* clock);
  ~Logging();
  // Inform of new event: three types of events: frame, packets and generic.
  // Frame events can be inserted with different parameters.
  void InsertFrameEvent(CastLoggingEvent event,
                        uint32 rtp_timestamp,
                        uint32 frame_id);
  // Size - Inserting the size implies that this is an encoded frame.
  void InsertFrameEventWithSize(CastLoggingEvent event,
                                uint32 rtp_timestamp,
                                uint32 frame_id,
                                int frame_size);
  // Render/playout delay
  void InsertFrameEventWithDelay(CastLoggingEvent event,
                                 uint32 rtp_timestamp,
                                 uint32 frame_id,
                                 base::TimeDelta delay);

  // Insert a packet event.
  void InsertPacketEvent(CastLoggingEvent event,
                         uint32 rtp_timestamp,
                         uint32 frame_id,
                         uint16 packet_id,
                         uint16 max_packet_id,
                         int size);

  void InsertGenericEvent(CastLoggingEvent event, int value);

  // Get log data.
  void GetRawFrameData(FrameLogMap frame_data);
  void GetRawPacketData(PacketLogMap packet_data);
  void GetRawGenericData(GenericLogMap generic_data);

  // Reset all log data (not flags).
  void Reset();

 private:
  base::WeakPtrFactory<Logging> weak_factory_;
  base::TickClock* const clock_;  // Not owned by this class.
  FrameLogMap frame_map_;
  PacketLogMap packet_map_;
  GenericLogMap generic_map_;

  DISALLOW_COPY_AND_ASSIGN(Logging);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_H_

