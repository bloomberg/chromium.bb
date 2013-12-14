// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_EVENT_EMITTER_STREAM_H_
#define LIBRARIES_NACL_IO_EVENT_EMITTER_STREAM_H_

#include "nacl_io/event_emitter.h"

#include "sdk_util/macros.h"
#include "sdk_util/scoped_ref.h"

namespace nacl_io {

class EventEmitterStream;
class FIFOInterface;
class MountNodeStream;

typedef sdk_util::ScopedRef<EventEmitterStream> ScopedEventEmitterStream;

class EventEmitterStream : public EventEmitter {
 public:
  EventEmitterStream();

  void AttachStream(MountNodeStream* stream);
  void DetachStream();

  MountNodeStream* stream() { return stream_; }

  uint32_t BytesInOutputFIFO();
  uint32_t SpaceInInputFIFO();

protected:
  virtual FIFOInterface* in_fifo() = 0;
  virtual FIFOInterface* out_fifo() = 0;
  void UpdateStatus_Locked();

protected:
  MountNodeStream* stream_;
  DISALLOW_COPY_AND_ASSIGN(EventEmitterStream);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_EVENT_EMITTER_STREAM_H_

