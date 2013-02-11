// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

namespace {

// Creates a FlagsAndLength.
FlagsAndLength CreateFlagsAndLength(SpdyControlFlags flags, size_t length) {
  DCHECK_EQ(0u, length & ~static_cast<size_t>(kLengthMask));
  FlagsAndLength flags_length;
  flags_length.length_ = htonl(static_cast<uint32>(length));
  DCHECK_EQ(0, flags & ~kControlFlagsMask);
  flags_length.flags_[0] = flags;
  return flags_length;
}

}  // namespace

SpdyFrameBuilder::SpdyFrameBuilder(size_t size)
    : buffer_(new char[size]),
      capacity_(size),
      length_(0) {
}

SpdyFrameBuilder::SpdyFrameBuilder(SpdyControlType type,
                                   SpdyControlFlags flags,
                                   int spdy_version,
                                   size_t size)
    : buffer_(new char[size]),
      capacity_(size),
      length_(0) {
  FlagsAndLength flags_length = CreateFlagsAndLength(
      flags, size - SpdyFrame::kHeaderSize);
  WriteUInt16(kControlFlagMask | spdy_version);
  WriteUInt16(type);
  WriteBytes(&flags_length, sizeof(flags_length));
}

SpdyFrameBuilder::SpdyFrameBuilder(SpdyStreamId stream_id,
                                   SpdyDataFlags flags,
                                   size_t size)
    : buffer_(new char[size]),
      capacity_(size),
      length_(0) {
  DCHECK_EQ(0u, stream_id & ~kStreamIdMask);
  WriteUInt32(stream_id);
  size_t length = size - SpdyFrame::kHeaderSize;
  DCHECK_EQ(0u, length & ~static_cast<size_t>(kLengthMask));
  FlagsAndLength flags_length;
  flags_length.length_ = htonl(length);
  DCHECK_EQ(0, flags & ~kDataFlagsMask);
  flags_length.flags_[0] = flags;
  WriteBytes(&flags_length, sizeof(flags_length));
}

SpdyFrameBuilder::~SpdyFrameBuilder() {
}

bool SpdyFrameBuilder::WriteBytes(const void* data, uint32 data_len) {
  if (data_len > kLengthMask) {
    DCHECK(false);
    return false;
  }

  size_t offset = length_;
  size_t needed_size = length_ + data_len;
  if (needed_size > capacity_) {
    DCHECK(false);
    return false;
  }

#ifdef ARCH_CPU_64_BITS
  DCHECK_LE(data_len, std::numeric_limits<uint32>::max());
#endif

  char* dest = buffer_.get() + offset;
  memcpy(dest, data, data_len);
  length_ += data_len;
  return true;
}

bool SpdyFrameBuilder::WriteString(const std::string& value) {
  if (value.size() > 0xffff) {
    DCHECK(false) << "Tried to write string with length > 16bit.";
    return false;
  }

  if (!WriteUInt16(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(), static_cast<uint16>(value.size()));
}

bool SpdyFrameBuilder::WriteStringPiece32(const base::StringPiece& value) {
  if (!WriteUInt32(value.size())) {
    return false;
  }

  return WriteBytes(value.data(), value.size());
}

}  // namespace net
