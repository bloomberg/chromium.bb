// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_DECRYPT_UTIL_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_DECRYPT_UTIL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"

namespace chromecast {
namespace media {

class CastDecryptConfig;
class DecryptContextImpl;

class DecoderBufferClear : public DecoderBufferBase {
 public:
  explicit DecoderBufferClear(scoped_refptr<DecoderBufferBase> buffer);

  // DecoderBufferBase implementation.
  StreamId stream_id() const override;
  int64_t timestamp() const override;
  void set_timestamp(base::TimeDelta timestamp) override;
  const uint8_t* data() const override;
  uint8_t* writable_data() const override;
  size_t data_size() const override;
  const CastDecryptConfig* decrypt_config() const override;
  bool end_of_stream() const override;
  scoped_refptr<::media::DecoderBuffer> ToMediaBuffer() const override;

 private:
  ~DecoderBufferClear() override;

  const scoped_refptr<DecoderBufferBase> buffer_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBufferClear);
};

using BufferDecryptedCB =
    base::OnceCallback<void(scoped_refptr<DecoderBufferBase>, bool)>;

// Create a new buffer which corresponds to the clear version of |buffer|.
// Note: the memory area corresponding to the ES data of the new buffer
// is the same as the ES data of |buffer| (for efficiency).
// After the |buffer_decrypted_cb| is called, |buffer| is left in a inconsistent
// state in the sense it has some decryption info but the ES data is now in
// clear.
void DecryptDecoderBuffer(scoped_refptr<DecoderBufferBase> buffer,
                          DecryptContextImpl* decrypt_ctxt,
                          BufferDecryptedCB buffer_decrypted_cb);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_DECRYPT_UTIL_H_
