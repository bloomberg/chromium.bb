// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_ENCODING_EVENT_SUBSCRIBER_H_
#define MEDIA_CAST_LOGGING_ENCODING_EVENT_SUBSCRIBER_H_

#include <map>

#include "base/memory/linked_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"
#include "media/cast/logging/raw_event_subscriber.h"

namespace media {
namespace cast {

typedef std::map<RtpTimestamp,
                 linked_ptr<media::cast::proto::AggregatedFrameEvent> >
    FrameEventMap;
typedef std::map<RtpTimestamp,
                 linked_ptr<media::cast::proto::AggregatedPacketEvent> >
    PacketEventMap;

// A RawEventSubscriber implementation that subscribes to events,
// encodes them in protocol buffer format, and aggregates them into a more
// compact structure.
class EncodingEventSubscriber : public RawEventSubscriber {
 public:
  // |event_media_type|: The subscriber will only process events that
  // corresponds to this type.
  // |max_frames|: How many events to keep in the frame / packet map.
  // This helps keep memory usage bounded.
  // Every time one of |OnReceive[Frame,Packet]Event()| is
  // called, it will check if the respective map size has exceeded |max_frames|.
  // If so, it will remove the oldest aggregated entry (ordered by RTP
  // timestamp).
  EncodingEventSubscriber(EventMediaType event_media_type, size_t max_frames);

  virtual ~EncodingEventSubscriber();

  // RawReventSubscriber implementations.
  virtual void OnReceiveFrameEvent(const FrameEvent& frame_event) OVERRIDE;
  virtual void OnReceivePacketEvent(const PacketEvent& packet_event) OVERRIDE;
  virtual void OnReceiveGenericEvent(const GenericEvent& generic_event)
      OVERRIDE;

  // Assigns frame events received so far to |frame_events| and clears them
  // from this object.
  void GetFrameEventsAndReset(FrameEventMap* frame_events);

  // Assigns packet events received so far to |packet_events| and clears them
  // from this object.
  void GetPacketEventsAndReset(PacketEventMap* packet_events);

 private:
  bool ShouldProcessEvent(CastLoggingEvent event);

  // Removes oldest entry from |frame_event_map_| (ordered by RTP timestamp).
  void TruncateFrameEventMapIfNeeded();

  // Removes oldest entry from |packet_event_map_| (ordered by RTP timestamp).
  void TruncatePacketEventMapIfNeeded();

  const EventMediaType event_media_type_;
  const size_t max_frames_;

  FrameEventMap frame_event_map_;
  PacketEventMap packet_event_map_;

  // All functions must be called on the main thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(EncodingEventSubscriber);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_ENCODING_EVENT_SUBSCRIBER_H_
