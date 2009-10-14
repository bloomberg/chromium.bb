// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some protocol structures for use with Flip.

#ifndef NET_FLIP_FLIP_PROTOCOL_H_
#define NET_FLIP_FLIP_PROTOCOL_H_

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include "base/basictypes.h"
#include "base/logging.h"
#include "flip_bitmasks.h"  // cross-google3 directory naming.


//  Data Frame Format
//  +----------------------------------+
//  |0|       Stream-ID (31bits)       |
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |
//  +----------------------------------+
//  |               Data               |
//  +----------------------------------+
//
//  Control Frame Format
//  +----------------------------------+
//  |1| Version(15bits) | Type(16bits) |
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |
//  +----------------------------------+
//  |               Data               |
//  +----------------------------------+
//
//  Control Frame: SYN_STREAM
//  +----------------------------------+
//  |1|000000000000001|0000000000000001|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 8
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+
//  |Pri| unused      | Length (16bits)|
//  +----------------------------------+
//
//  Control Frame: SYN_REPLY
//  +----------------------------------+
//  |1|000000000000001|0000000000000010|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 8
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+
//  | unused (16 bits)| Length (16bits)|
//  +----------------------------------+
//
//  Control Frame: FIN_STREAM
//  +----------------------------------+
//  |1|000000000000001|0000000000000011|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 4
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+
//  |             Status (32 bits)     |
//  +----------------------------------+
//
//  Control Frame: SetMaxStreams
//  +----------------------------------+
//  |1|000000000000001|0000000000000100|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 4
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+

// TODO(fenix): add ChangePriority support.

namespace flip {

// This implementation of Flip is version 1.
const int kFlipProtocolVersion = 1;

// Note: all protocol data structures are on-the-wire format.  That means that
//       data is stored in network-normalized order.  Readers must use the
//       accessors provided or call ntohX() functions.

// Types of Flip Control Frames.
typedef enum {
  SYN_STREAM = 1,
  SYN_REPLY,
  FIN_STREAM,
  NOOP
} FlipControlType;

// Flags on data packets
typedef enum {
  DATA_FLAG_FIN = 1,
  DATA_FLAG_COMPRESSED = 2  // TODO(mbelshe): remove me.
} FlipDataFlags;

// Flags on control packets
typedef enum {
  CONTROL_FLAG_FIN = 1
} FlipControlFlags;

// A FLIP stream id is a 31 bit entity.
typedef uint32 FlipStreamId;

// FLIP Priorities. (there are only 2 bits)
#define FLIP_PRIORITY_LOWEST 3
#define FLIP_PRIORITY_HIGHEST 0

// A special structure for the 8 bit flags and 24 bit length fields.
typedef union {
  uint8 flags_[4]; // 8 bits
  uint32 length_;  // 24 bits
} FlagsLength;

// All Flip Frame types derive from the FlipFrame struct.
typedef struct {
  uint8 flags() const { return flags_length_.flags_[0]; }
  void set_flags(uint8 flags) { flags_length_.flags_[0] = flags; }

  uint32 length() const { return ntohl(flags_length_.length_) & kLengthMask; }
  void set_length(uint32 length) {
    DCHECK((length & ~kLengthMask) == 0);
    length = htonl(length & kLengthMask);
    flags_length_.length_ = (flags() << 6) | length;
  }

  bool is_control_frame() const {
    return (ntohs(control_.version_) & kControlFlagMask) == kControlFlagMask;
  }

 protected:
  union {
    struct {
      uint16 version_;
      uint16 type_;
    } control_;
    struct {
      FlipStreamId stream_id_;
    } data_;
  };
  FlagsLength flags_length_;
} FlipFrame;

// A Data Frame.
typedef struct : public FlipFrame {
  FlipStreamId stream_id() const {
      return ntohl(data_.stream_id_) & kStreamIdMask;
  }
  void set_stream_id(FlipStreamId id) { data_.stream_id_ = htonl(id); }
} FlipDataFrame;

// A Control Frame.
typedef struct : public FlipFrame {
  uint16 version() const {
    const int kVersionMask = 0x7fff;
    return ntohs(control_.version_) & kVersionMask;
  }
  FlipControlType type() const {
    uint16 type = ntohs(control_.type_);
    DCHECK(type >= SYN_STREAM && type <= NOOP);
    return static_cast<FlipControlType>(type);
  }
  FlipStreamId stream_id() const { return ntohl(stream_id_) & kStreamIdMask; }

 private:
  FlipStreamId stream_id_;
} FlipControlFrame;

// A SYN_STREAM frame.
typedef struct FlipSynStreamControlFrame : public FlipControlFrame {
  uint8 priority() const { return (priority_ & kPriorityMask) >> 6; }
  // The number of bytes in the header block beyond the frame header length.
  int header_block_len() const { return length() - kHeaderBlockOffset; }
  const char* header_block() const {
    return reinterpret_cast<const char*>(this) +
        sizeof(FlipFrame) + kHeaderBlockOffset;
  }
 private:
  // Offset from the end of the FlipControlFrame to the FlipHeaderBlock.
  static const int kHeaderBlockOffset = 6;
  uint8 priority_;
  uint8 unused_;
  // Variable FlipHeaderBlock here.
} FlipSynStreamControlFrame;

// A SYN_REPLY frame.
typedef struct FlipSynReplyControlFrame : public FlipControlFrame {
  int header_block_len() const { return length() - kHeaderBlockOffset; }
  const char* header_block() const {
    return reinterpret_cast<const char*>(this) +
        sizeof(FlipFrame) + kHeaderBlockOffset;
  }

 private:
  // Offset from the end of the FlipControlFrame to the FlipHeaderBlock.
  static const int kHeaderBlockOffset = 6;
  uint16 unused_;
  // Variable FlipHeaderBlock here.
} FlipSynReplyControlFrame;

// A FIN_STREAM frame.
typedef struct FlipFinStreamControlFrame : public FlipControlFrame {
  uint32 status() const { return ntohl(status_); }
  void set_status(int id) { status_ = htonl(id); }
 private:
  uint32 status_;
} FlipFinStreamControlFrame;

}  // namespace flip

#endif  // NET_FLIP_FLIP_PROTOCOL_H_

