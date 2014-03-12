// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The serialization format is as follows:
//   8-bit integer describing |is_audio|.
//   32-bit integer describing |first_rtp_timestamp|.
//   32-bit integer describing number of frame events.
//   (The following repeated for number of frame events):
//     16-bit integer describing the following AggregatedFrameEvent proto size
//         in bytes.
//     The AggregatedFrameEvent proto.
//   32-bit integer describing number of packet events.
//   (The following repeated for number of packet events):
//     16-bit integer describing the following AggregatedPacketEvent proto
//         size in bytes.
//     The AggregatedPacketEvent proto.

#include "media/cast/logging/log_serializer.h"

#include "base/big_endian.h"
#include "third_party/zlib/zlib.h"

namespace media {
namespace cast {

namespace {

using media::cast::proto::AggregatedFrameEvent;
using media::cast::proto::AggregatedPacketEvent;
using media::cast::proto::LogMetadata;

// Use 30MB of temp buffer to hold uncompressed data if |compress| is true.
const int kMaxUncompressedBytes = 30 * 1000 * 1000;

bool DoSerializeEvents(const LogMetadata& metadata,
                       const FrameEventMap& frame_events,
                       const PacketEventMap& packet_events,
                       const int max_output_bytes,
                       char* output,
                       int* output_bytes) {
  base::BigEndianWriter writer(output, max_output_bytes);

  int proto_size = metadata.ByteSize();
  if (!writer.WriteU16(proto_size))
    return false;
  if (!metadata.SerializeToArray(writer.ptr(), writer.remaining()))
    return false;
  if (!writer.Skip(proto_size))
    return false;

  for (media::cast::FrameEventMap::const_iterator it = frame_events.begin();
       it != frame_events.end();
       ++it) {
    proto_size = it->second->ByteSize();

    // Write size of the proto, then write the proto.
    if (!writer.WriteU16(proto_size))
      return false;
    if (!it->second->SerializeToArray(writer.ptr(), writer.remaining()))
      return false;
    if (!writer.Skip(proto_size))
      return false;
  }

  // Write packet events.
  for (media::cast::PacketEventMap::const_iterator it = packet_events.begin();
       it != packet_events.end();
       ++it) {
    proto_size = it->second->ByteSize();

    // Write size of the proto, then write the proto.
    if (!writer.WriteU16(proto_size))
      return false;
    if (!it->second->SerializeToArray(writer.ptr(), writer.remaining()))
      return false;
    if (!writer.Skip(proto_size))
      return false;
  }

  *output_bytes = max_output_bytes - writer.remaining();
  return true;
}

bool Compress(char* uncompressed_buffer,
              int uncompressed_bytes,
              int max_output_bytes,
              char* output,
              int* output_bytes) {
  z_stream stream = {0};
  int result = deflateInit2(&stream,
                            Z_DEFAULT_COMPRESSION,
                            Z_DEFLATED,
                            // 16 is added to produce a gzip header + trailer.
                            MAX_WBITS + 16,
                            8,  // memLevel = 8 is default.
                            Z_DEFAULT_STRATEGY);
  DCHECK_EQ(Z_OK, result);

  stream.next_in = reinterpret_cast<uint8*>(uncompressed_buffer);
  stream.avail_in = uncompressed_bytes;
  stream.next_out = reinterpret_cast<uint8*>(output);
  stream.avail_out = max_output_bytes;

  // Do a one-shot compression. This will return Z_STREAM_END only if |output|
  // is large enough to hold all compressed data.
  result = deflate(&stream, Z_FINISH);
  bool success = (result == Z_STREAM_END);

  if (!success)
    DVLOG(2) << "deflate() failed. Result: " << result;

  result = deflateEnd(&stream);
  DCHECK(result == Z_OK || result == Z_DATA_ERROR);

  if (success)
    *output_bytes = max_output_bytes - stream.avail_out;

  return success;
}

}  // namespace

bool SerializeEvents(const LogMetadata& log_metadata,
                     const FrameEventMap& frame_events,
                     const PacketEventMap& packet_events,
                     bool compress,
                     int max_output_bytes,
                     char* output,
                     int* output_bytes) {
  DCHECK_GT(max_output_bytes, 0);
  DCHECK(output);
  DCHECK(output_bytes);

  if (compress) {
    // Allocate a reasonably large temp buffer to hold uncompressed data.
    scoped_ptr<char[]> uncompressed_buffer(new char[kMaxUncompressedBytes]);
    int uncompressed_bytes;
    bool success = DoSerializeEvents(log_metadata,
                                     frame_events,
                                     packet_events,
                                     kMaxUncompressedBytes,
                                     uncompressed_buffer.get(),
                                     &uncompressed_bytes);
    if (!success)
      return false;
    return Compress(uncompressed_buffer.get(),
                    uncompressed_bytes,
                    max_output_bytes,
                    output,
                    output_bytes);
  } else {
    return DoSerializeEvents(log_metadata,
                             frame_events,
                             packet_events,
                             max_output_bytes,
                             output,
                             output_bytes);
  }
}

}  // namespace cast
}  // namespace media
