// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_DECODER_BUFFER_BASE_H_
#define CHROMECAST_MEDIA_CMA_BASE_DECODER_BUFFER_BASE_H_

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chromecast/public/media/stream_id.h"

namespace chromecast {
namespace media {
class CastDecryptConfig;

// DecoderBufferBase exposes only the properties of an audio/video buffer.
// The way a DecoderBufferBase is created and organized in memory
// is left as a detail of the implementation of derived classes.
class DecoderBufferBase
    : public base::RefCountedThreadSafe<DecoderBufferBase> {
 public:
  DecoderBufferBase() {}

  // Returns the stream id of this decoder buffer belonging to. it's optional
  // and default value is kPrimary.
  virtual StreamId stream_id() const = 0;

  // Returns the PTS of the frame.
  virtual base::TimeDelta timestamp() const = 0;

  // Sets the PTS of the frame.
  virtual void set_timestamp(base::TimeDelta timestamp) = 0;

  // Gets the frame data.
  virtual const uint8* data() const = 0;
  virtual uint8* writable_data() const = 0;

  // Returns the size of the frame in bytes.
  virtual size_t data_size() const = 0;

  // Returns the decrypt configuration.
  // Returns NULL if the buffer has no decrypt info.
  virtual const CastDecryptConfig* decrypt_config() const = 0;

  // Indicate if this is a special frame that indicates the end of the stream.
  // If true, functions to access the frame content cannot be called.
  virtual bool end_of_stream() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<DecoderBufferBase>;
  virtual ~DecoderBufferBase() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DecoderBufferBase);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_DECODER_BUFFER_BASE_H_
