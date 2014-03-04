// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOG_SERIALIZER_H_
#define MEDIA_CAST_LOGGING_LOG_SERIALIZER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/cast/logging/encoding_event_subscriber.h"

namespace media {
namespace cast {

// The max allowed size of serialized log.
const int kMaxSerializedLogBytes = 20 * 1000 * 1000;

// This class takes FrameEventMap and PacketEventMap returned from an
// EncodingEventSubscriber and serialize them. An instance of this class
// can take input from multiple EncodingEventSubscribers (which represents
// different streams).
// Usage:
// 1. Construct a LogSerializer with a predefined maximum serialized length.
// 2. For each set of FrameEventMap and PacketEventMap returned by
//    EncodingEventSubscriber installed on each stream, call
//    |SerializeEventsForStream()| along with the stream id. Check the returned
//    value to see if serialization succeeded.
// 3. Call |GetSerializedLogAndReset()| to get the result.
// 4. (Optional) Call |GetSerializedLengthSoFar()| between each serialize call.
class LogSerializer {
 public:
  // Constructs a LogSerializer that caps size of serialized message to
  // |max_serialized_bytes|.
  explicit LogSerializer(const int max_serialized_bytes);
  ~LogSerializer();

  // Serialize |frame_events|, |packet_events|, |first_rtp_timestamp|
  // returned from EncodingEventSubscriber. |is_audio| indicates whether the
  // events are from an audio or video stream.
  //
  // Returns |true| if serialization is successful. This function
  // returns |false| if the serialized string will exceed |kMaxSerializedsize|.
  //
  // This may be called multiple times with different streams and events before
  // calling |GetSerializedString()|.
  //
  // See .cc file for format specification.
  bool SerializeEventsForStream(bool is_audio,
                                const FrameEventMap& frame_events,
                                const PacketEventMap& packet_events,
                                const RtpTimestamp first_rtp_timestamp);

  // Gets a string of serialized events up to the last successful call to
  // |SerializeEventsForStream()| and resets it.
  scoped_ptr<std::string> GetSerializedLogAndReset();

  // Returns the length of the serialized string since last
  // successful serialization / reset.
  int GetSerializedLength() const;

 private:
  scoped_ptr<std::string> serialized_log_so_far_;
  int index_so_far_;
  const int max_serialized_bytes_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOG_SERIALIZER_H_
