// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_CAST_DECODER_BUFFER_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BASE_CAST_DECODER_BUFFER_IMPL_H_

#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"

namespace chromecast {
namespace media {

// Implementation of public DecoderBuffer interface.
// Holds a (refcounted) DecoderBufferBase.  This way, ownership throughout
// the pipeline is refcounted, but without exposing RefCountedBase in
// the public API.
class CastDecoderBufferImpl : public CastDecoderBuffer {
 public:
  CastDecoderBufferImpl(const scoped_refptr<DecoderBufferBase>& buffer);
  ~CastDecoderBufferImpl() override;

  // Returns the stream id of this decoder buffer belonging to. it's optional
  // and default value is kPrimary.
  StreamId stream_id() const override;

  // Returns the PTS of the frame in microseconds.
  int64_t timestamp() const override;

  // Gets the frame data.
  const uint8* data() const override;

  // Returns the size of the frame in bytes.
  size_t data_size() const override;

  // Returns the decrypt configuration.
  // Returns NULL if the buffer has no decrypt info.
  const CastDecryptConfig* decrypt_config() const override;

  // Indicate if this is a special frame that indicates the end of the stream.
  // If true, functions to access the frame content cannot be called.
  bool end_of_stream() const override;

  const scoped_refptr<DecoderBufferBase>& buffer() const;
  void set_buffer(const scoped_refptr<DecoderBufferBase>& buffer);

 private:
  scoped_refptr<DecoderBufferBase> buffer_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_CAST_DECODER_BUFFER_BASE_H_
