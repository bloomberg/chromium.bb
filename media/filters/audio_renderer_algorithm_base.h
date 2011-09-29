// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioRendererAlgorithmBase provides an interface for algorithms that modify
// playback speed. ARAB owns a BufferQueue which hides Buffer boundaries from
// subclasses and allows them to access data by byte. Subclasses must implement
// the following method:
//
//   FillBuffer() - fills the buffer passed to it & returns how many bytes
//                  copied.
//
// The general assumption is that the owner of this class will provide us with
// Buffers and a playback speed, and we will fill an output buffer when our
// owner requests it. If we needs more Buffers, we will query our owner via
// a callback passed during construction. This should be a nonblocking call.
// When the owner has a Buffer ready for us, it calls EnqueueBuffer().
//
// Exectution of ARAB is thread-unsafe. This class should be used as
// the guts of AudioRendererBase, which should lock calls into us so
// enqueues and processes do not cause an unpredictable |queue_| size.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_BASE_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_BASE_H_

#include <deque>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/seekable_buffer.h"

namespace media {

class Buffer;

class MEDIA_EXPORT AudioRendererAlgorithmBase {
 public:
  AudioRendererAlgorithmBase();
  virtual ~AudioRendererAlgorithmBase();

  // Checks validity of audio parameters and takes ownership of |callback|.
  virtual void Initialize(int channels,
                          int sample_rate,
                          int sample_bits,
                          float initial_playback_rate,
                          const base::Closure& callback);

  // Implement this strategy method in derived classes. Tries to fill |length|
  // bytes of |dest| with possibly scaled data from our |queue_|. Returns the
  // number of bytes copied into |dest|.
  virtual uint32 FillBuffer(uint8* dest, uint32 length) = 0;

  // Clears |queue_|.
  virtual void FlushBuffers();

  // Returns the time of the next byte in our data or kNoTimestamp if current
  // time is unknown.
  virtual base::TimeDelta GetTime();

  // Enqueues a buffer. It is called from the owner of the algorithm after a
  // read completes.
  virtual void EnqueueBuffer(Buffer* buffer_in);

  // Getter/setter for |playback_rate_|.
  virtual float playback_rate();
  virtual void set_playback_rate(float new_rate);

  // Returns whether |queue_| is empty.
  virtual bool IsQueueEmpty();

  // Returns true if we have enough data
  virtual bool IsQueueFull();

  // Returns the number of bytes left in |queue_|.
  virtual uint32 QueueSize();

 protected:
  // Advances |queue_|'s internal pointer by |bytes|.
  void AdvanceInputPosition(uint32 bytes);

  // Tries to copy |bytes| bytes from |queue_| to |dest|. Returns the number of
  // bytes successfully copied.
  uint32 CopyFromInput(uint8* dest, uint32 bytes);

  // Number of audio channels.
  virtual int channels();

  // Sample rate in hertz.
  virtual int sample_rate();

  // Number of bytes per sample per channel.
  virtual int sample_bytes();

 private:
  // Audio properties.
  int channels_;
  int sample_rate_;
  int sample_bytes_;

  // Used by algorithm to scale output.
  float playback_rate_;

  // Used to request more data.
  base::Closure request_read_callback_;

  // Queued audio data.
  SeekableBuffer queue_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererAlgorithmBase);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_BASE_H_
