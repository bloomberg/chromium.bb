// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_FRAME_BUILDER_H_
#define NET_SPDY_SPDY_FRAME_BUILDER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/sys_byteorder.h"
#include "net/base/net_export.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

class SpdyFramer;

// This class provides facilities for basic binary value packing
// into Spdy frames.
//
// The SpdyFrameBuilder supports appending primitive values (int, string, etc)
// to a frame instance.  The SpdyFrameBuilder grows its internal memory buffer
// dynamically to hold the sequence of primitive values.   The internal memory
// buffer is exposed as the "data" of the SpdyFrameBuilder.
class NET_EXPORT_PRIVATE SpdyFrameBuilder {
 public:
  // Initializes a SpdyFrameBuilder with a buffer of given size
  explicit SpdyFrameBuilder(size_t size);

  ~SpdyFrameBuilder();

  // Returns the size of the SpdyFrameBuilder's data.
  size_t length() const { return length_; }

  // Returns a writeable buffer of given size in bytes, to be appended to the
  // currently written frame. Does bounds checking on length but does not
  // increment the underlying iterator. To do so, consumers should subsequently
  // call Seek().
  // In general, consumers should use Write*() calls instead of this.
  // Returns NULL on failure.
  char* GetWritableBuffer(size_t length);

  // Seeks forward by the given number of bytes. Useful in conjunction with
  // GetWriteableBuffer() above.
  bool Seek(size_t length);

  // Populates this frame with a SPDY control frame header using
  // version-specific information from the |framer| and length information from
  // capacity_.
  bool WriteControlFrameHeader(const SpdyFramer& framer,
                               SpdyControlType type,
                               uint8 flags);

  // Populates this frame with a SPDY data frame header using version-specific
  // information from the |framer| and length information from capacity_.
  bool WriteDataFrameHeader(const SpdyFramer& framer,
                            SpdyStreamId stream_id,
                            SpdyDataFlags flags);

  // Takes the buffer from the SpdyFrameBuilder.
  SpdyFrame* take() {
    SpdyFrame* rv = new SpdyFrame(buffer_.release(), length_, true);
    capacity_ = 0;
    length_ = 0;
    return rv;
  }

  // Methods for adding to the payload.  These values are appended to the end
  // of the SpdyFrameBuilder payload. Note - binary integers are converted from
  // host to network form.
  bool WriteUInt8(uint8 value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt16(uint16 value) {
    value = htons(value);
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt32(uint32 value) {
    value = htonl(value);
    return WriteBytes(&value, sizeof(value));
  }
  // TODO(hkhalil) Rename to WriteStringPiece16().
  bool WriteString(const std::string& value);
  bool WriteStringPiece32(const base::StringPiece& value);
  bool WriteBytes(const void* data, uint32 data_len);

  // Update (in-place) the length field in the frame being built to reflect the
  // current actual length of bytes written to said frame through this builder.
  // The framer parameter is used to determine version-specific location and
  // size information of the length field to be written, and must be initialized
  // with the correct version for the frame being written.
  bool RewriteLength(const SpdyFramer& framer);

  // Update (in-place) the length field in the frame being built to reflect the
  // given length.
  // The framer parameter is used to determine version-specific location and
  // size information of the length field to be written, and must be initialized
  // with the correct version for the frame being written.
  bool OverwriteLength(const SpdyFramer& framer, size_t length);

 protected:
  const char* end_of_payload() const { return buffer_.get() + length_; }

 private:
  // Checks to make sure that there is an appropriate amount of space for a
  // write of given size, in bytes.
  bool CanWrite(size_t length) const;

  scoped_ptr<char[]> buffer_;
  size_t capacity_;  // Allocation size of payload, set by constructor.
  size_t length_;    // Current length of the buffer.
};

}  // namespace net

#endif  // NET_SPDY_SPDY_FRAME_BUILDER_H_
