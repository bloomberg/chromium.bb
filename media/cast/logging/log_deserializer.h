// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOG_DESERIALIZER_H_
#define MEDIA_CAST_LOGGING_LOG_DESERIALIZER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/cast/logging/encoding_event_subscriber.h"

namespace media {
namespace cast {

// This function takes the output of LogSerializer and deserializes it into
// its original format. Returns true if deserialization is successful. All
// output arguments are valid if this function returns true.
// |data|: Serialized event logs with length |data_bytes|.
// |compressed|: true if |data| is compressed in gzip format.
// |log_metadata|: This will be populated with deserialized LogMetadata proto.
// |frame_events|: This will be populated with deserialized frame events.
// |packet_events|: This will be populated with deserialized packet events.
bool DeserializeEvents(char* data,
                       int data_bytes,
                       bool compressed,
                       media::cast::proto::LogMetadata* log_metadata,
                       FrameEventMap* frame_events,
                       PacketEventMap* packet_events);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOG_DESERIALIZER_H_
