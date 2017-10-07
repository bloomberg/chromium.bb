// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/common/profiling/memlog_stream.h"
#include "chrome/profiling/address.h"

namespace profiling {

// A log receiver is a sink for parsed allocation events. See also
// MemlogStreamReceiver which is for the unparsed data blocks.
class MemlogReceiver {
 public:
  virtual ~MemlogReceiver() {}

  virtual void OnHeader(const StreamHeader& header) = 0;
  virtual void OnAlloc(const AllocPacket& alloc_packet,
                       std::vector<Address>&& stack,
                       std::string&& context) = 0;
  virtual void OnFree(const FreePacket& free_packet) = 0;
  virtual void OnBarrier(const BarrierPacket& barrier_packet) = 0;
  virtual void OnComplete() = 0;
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_H_
