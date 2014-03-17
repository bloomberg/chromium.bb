// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DECODER_H_
#define MEDIA_BASE_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

class AudioBuffer;
class DemuxerStream;

class MEDIA_EXPORT AudioDecoder {
 public:
  // Status codes for decode operations.
  // TODO(rileya): Now that both AudioDecoder and VideoDecoder Status enums
  // match, break them into a decoder_status.h.
  enum Status {
    kOk,  // We're all good.
    kAborted,  // We aborted as a result of Stop() or Reset().
    kNotEnoughData,  // Not enough data to produce a video frame.
    kDecodeError,  // A decoding error occurred.
    kDecryptError  // Decrypting error happened.
  };

  AudioDecoder();
  virtual ~AudioDecoder();

  // Initializes an AudioDecoder with the given DemuxerStream, executing the
  // callback upon completion.
  // statistics_cb is used to update global pipeline statistics.
  virtual void Initialize(const AudioDecoderConfig& config,
                          const PipelineStatusCB& status_cb) = 0;

  // Requests samples to be decoded and returned via the provided callback.
  // Only one decode may be in flight at any given time.
  //
  // Implementations guarantee that the callback will not be called from within
  // this method.
  //
  // Non-NULL sample buffer pointers will contain decoded audio data or may
  // indicate the end of the stream. A NULL buffer pointer indicates an aborted
  // Decode().
  typedef base::Callback<void(Status, const scoped_refptr<AudioBuffer>&)>
      DecodeCB;
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) = 0;

  // Some AudioDecoders will queue up multiple AudioBuffers from a single
  // DecoderBuffer, if we have any such queued buffers this will return the next
  // one. Otherwise we return a NULL AudioBuffer.
  virtual scoped_refptr<AudioBuffer> GetDecodeOutput();

  // Resets decoder state, dropping any queued encoded data.
  virtual void Reset(const base::Closure& closure) = 0;

  // Stops decoder, fires any pending callbacks and sets the decoder to an
  // uninitialized state. An AudioDecoder cannot be re-initialized after it has
  // been stopped.
  // Note that if Initialize() has been called, Stop() must be called and
  // complete before deleting the decoder.
  virtual void Stop(const base::Closure& closure) = 0;

  // Returns various information about the decoded audio format.
  virtual int bits_per_channel() = 0;
  virtual ChannelLayout channel_layout() = 0;
  virtual int samples_per_second() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoder);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DECODER_H_
