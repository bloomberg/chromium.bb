// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef NET_QUIC_QUIC_WRITE_BLOCKED_LIST_H_
#define NET_QUIC_QUIC_WRITE_BLOCKED_LIST_H_

#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"
#include "net/spdy/write_blocked_list.h"

namespace net {

// Keeps tracks of the QUIC streams that have data to write, sorted by
// priority.  QUIC stream priority order is:
// Crypto stream > Headers stream > Data streams by requested priority.
class NET_EXPORT_PRIVATE QuicWriteBlockedList {
 private:
  typedef WriteBlockedList<QuicStreamId> QuicWriteBlockedListBase;

 public:
  static const QuicPriority kHighestPriority;
  static const QuicPriority kLowestPriority;

  QuicWriteBlockedList();
  ~QuicWriteBlockedList();

  bool HasWriteBlockedStreams() const {
    return crypto_stream_blocked_ ||
        headers_stream_blocked_ ||
        base_write_blocked_list_.HasWriteBlockedStreams();
  }

  size_t NumBlockedStreams() const {
    size_t num_blocked = base_write_blocked_list_.NumBlockedStreams();
    if (crypto_stream_blocked_) {
      ++num_blocked;
    }
    if (headers_stream_blocked_) {
      ++num_blocked;
    }

    return num_blocked;
  }

  QuicStreamId PopFront() {
    if (crypto_stream_blocked_) {
      crypto_stream_blocked_ = false;
      return kCryptoStreamId;
    } else if (headers_stream_blocked_) {
      headers_stream_blocked_ = false;
      return kHeadersStreamId;
    } else {
      SpdyPriority priority =
          base_write_blocked_list_.GetHighestPriorityWriteBlockedList();
      return base_write_blocked_list_.PopFront(priority);
    }
  }

  // TODO(avd) Remove version argument when QUIC_VERSION_12 is deprecated.
  void PushBack(QuicStreamId stream_id,
                QuicPriority priority,
                QuicVersion version) {
    if (stream_id == kCryptoStreamId) {
      DCHECK_EQ(kHighestPriority, priority);
      // TODO(avd) Add DCHECK(!crypto_stream_blocked_)
      crypto_stream_blocked_ = true;
    } else if (version > QUIC_VERSION_12 &&
               stream_id == kHeadersStreamId) {
      DCHECK_EQ(kHighestPriority, priority);
      // TODO(avd) Add DCHECK(!headers_stream_blocked_);
      headers_stream_blocked_ = true;
    } else {
      base_write_blocked_list_.PushBack(
          stream_id, static_cast<SpdyPriority>(priority));
    }
  }

 private:
  QuicWriteBlockedListBase base_write_blocked_list_;
  bool crypto_stream_blocked_;
  bool headers_stream_blocked_;

  DISALLOW_COPY_AND_ASSIGN(QuicWriteBlockedList);
};

}  // namespace net


#endif  // NET_QUIC_QUIC_WRITE_BLOCKED_LIST_H_
