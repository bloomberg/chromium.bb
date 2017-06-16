// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_STREAM_RECEIVER_H_
#define CHROME_PROFILING_MEMLOG_STREAM_RECEIVER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace profiling {

// A stream receiver is a sink for unparsed bytes. See also LogReceiver.
class MemlogStreamReceiver
    : public base::RefCountedThreadSafe<MemlogStreamReceiver> {
 public:
  MemlogStreamReceiver() {}
  virtual ~MemlogStreamReceiver() {}

  // Returns true on success, false on unrecoverable error (in which case no
  // more blocks will be sent). May take a ref to the block, so the caller
  // should not modify it later.
  virtual void OnStreamData(std::unique_ptr<char[]> data, size_t sz) = 0;

  virtual void OnStreamComplete() = 0;
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_STREAM_RECEIVER_H_
