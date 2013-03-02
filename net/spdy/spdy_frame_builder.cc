// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_frame_builder.h"

#include <limits>

#include "base/logging.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

namespace {

// A special structure for the 8 bit flags and 24 bit length fields.
union FlagsAndLength {
  uint8 flags_[4];  // 8 bits
  uint32 length_;   // 24 bits
};

// Creates a FlagsAndLength.
FlagsAndLength CreateFlagsAndLength(uint8 flags, size_t length) {
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

SpdyFrameBuilder::~SpdyFrameBuilder() {
}

char* SpdyFrameBuilder::GetWritableBuffer(size_t length) {
  if (!CanWrite(length)) {
    return NULL;
  }
  return buffer_.get() + length_;
}

bool SpdyFrameBuilder::Seek(size_t length) {
  if (!CanWrite(length)) {
    return false;
  }

  length_ += length;
  return true;
}

bool SpdyFrameBuilder::WriteControlFrameHeader(const SpdyFramer& framer,
                                               SpdyControlType type,
                                               uint8 flags) {
  bool success = true;
  if (framer.protocol_version() < 4) {
    FlagsAndLength flags_length = CreateFlagsAndLength(
        flags, capacity_ - framer.GetControlFrameHeaderSize());
    success &= WriteUInt16(kControlFlagMask | framer.protocol_version());
    success &= WriteUInt16(type);
    success &= WriteBytes(&flags_length, sizeof(flags_length));
  } else {
    DCHECK_GT(1u<<16, capacity_);  // Make sure length fits in 2B.
    success &= WriteUInt16(capacity_);
    success &= WriteUInt8(type);
    success &= WriteUInt8(flags);
  }
  DCHECK_EQ(framer.GetControlFrameHeaderSize(), length());
  return success;
}

bool SpdyFrameBuilder::WriteDataFrameHeader(const SpdyFramer& framer,
                                            SpdyStreamId stream_id,
                                            SpdyDataFlags flags) {
  DCHECK_EQ(0u, stream_id & ~kStreamIdMask);
  bool success = true;
  if (framer.protocol_version() < 4) {
    success &= WriteUInt32(stream_id);
    size_t length_field = capacity_ - framer.GetDataFrameMinimumSize();
    DCHECK_EQ(0u, length_field & ~static_cast<size_t>(kLengthMask));
    FlagsAndLength flags_length;
    flags_length.length_ = htonl(length_field);
    DCHECK_EQ(0, flags & ~kDataFlagsMask);
    flags_length.flags_[0] = flags;
    success &= WriteBytes(&flags_length, sizeof(flags_length));
  } else {
    DCHECK_GT(1u<<16, capacity_);  // Make sure length fits in 2B.
    success &= WriteUInt16(capacity_);
    success &= WriteUInt8(0);
    success &= WriteUInt8(flags);
    success &= WriteUInt32(stream_id);
  }
  DCHECK_EQ(framer.GetDataFrameMinimumSize(), length());
  return success;
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

bool SpdyFrameBuilder::WriteBytes(const void* data, uint32 data_len) {
  if (!CanWrite(data_len)) {
    return false;
  }

  char* dest = GetWritableBuffer(data_len);
  memcpy(dest, data, data_len);
  Seek(data_len);
  return true;
}

bool SpdyFrameBuilder::RewriteLength(const SpdyFramer& framer) {
  if (framer.protocol_version() < 4) {
    return OverwriteLength(framer,
                           length_ - framer.GetControlFrameHeaderSize());
  } else {
    return OverwriteLength(framer, length_);
  }
}

bool SpdyFrameBuilder::OverwriteLength(const SpdyFramer& framer,
                                       size_t length) {
  bool success = false;
  const size_t old_length = length_;

  if (framer.protocol_version() < 4) {
    FlagsAndLength flags_length = CreateFlagsAndLength(
        0,  // We're not writing over the flags value anyway.
        length);

    // Write into the correct location by temporarily faking the offset.
    length_ = 5;  // Offset at which the length field occurs.
    success = WriteBytes(reinterpret_cast<char*>(&flags_length) + 1,
                         sizeof(flags_length) - 1);
  } else {
    length_ = 0;
    success = WriteUInt16(length);
  }

  length_ = old_length;
  return success;
}

bool SpdyFrameBuilder::CanWrite(size_t length) const {
  if (length > kLengthMask) {
    DCHECK(false);
    return false;
  }

  if (length_ + length > capacity_) {
    DCHECK(false);
    return false;
  }

  return true;
}

}  // namespace net
