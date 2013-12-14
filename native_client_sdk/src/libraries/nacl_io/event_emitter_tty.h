// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_EVENT_EMITTER_TTY_H_
#define LIBRARIES_NACL_IO_EVENT_EMITTER_TTY_H_

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

#include "nacl_io/event_emitter_stream.h"
#include "nacl_io/fifo_char.h"
#include "nacl_io/fifo_null.h"

#include "sdk_util/auto_lock.h"
#include "sdk_util/macros.h"

namespace nacl_io {

class EventEmitterTTY;
typedef sdk_util::ScopedRef<EventEmitterTTY> ScopedEventEmitterTTY;

class EventEmitterTTY : public EventEmitterStream {
 public:
  explicit EventEmitterTTY(size_t size);

  size_t Read_Locked(char* data, size_t len);
  size_t Write_Locked(const char* data, size_t len);

 protected:
  virtual FIFOChar* in_fifo() { return &fifo_; }
  virtual FIFONull* out_fifo() { return &null_; }

 private:
  FIFOChar fifo_;
  FIFONull null_;
  DISALLOW_COPY_AND_ASSIGN(EventEmitterTTY);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_EVENT_EMITTER_PIPE_H_

