// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_DECODER_H_
#define MEDIA_BASE_VIDEO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "ui/gfx/size.h"

namespace media {

class DecoderBuffer;
class VideoDecoderConfig;
class VideoFrame;

class MEDIA_EXPORT VideoDecoder {
 public:
  // Status codes for decode operations on VideoDecoder.
  // TODO(rileya): Now that both AudioDecoder and VideoDecoder Status enums
  // match, break them into a decoder_status.h.
  enum Status {
    kOk,  // Everything went as planned.
    kAborted,  // Decode was aborted as a result of Reset() being called.
    kNotEnoughData,  // Not enough data to produce a video frame.
    kDecodeError,  // Decoding error happened.
    kDecryptError  // Decrypting error happened.
  };

  VideoDecoder();
  virtual ~VideoDecoder();

  // Initializes a VideoDecoder with the given |config|, executing the
  // |status_cb| upon completion.
  //
  // Note:
  // 1) The VideoDecoder will be reinitialized if it was initialized before.
  //    Upon reinitialization, all internal buffered frames will be dropped.
  // 2) This method should not be called during pending decode, reset or stop.
  // 3) No VideoDecoder calls except for Stop() should be made before
  //    |status_cb| is executed.
  virtual void Initialize(const VideoDecoderConfig& config,
                          const PipelineStatusCB& status_cb) = 0;

  // Requests a |buffer| to be decoded. The status of the decoder and decoded
  // frame are returned via the provided callback. Only one decode may be in
  // flight at any given time.
  //
  // Implementations guarantee that the callback will not be called from within
  // this method.
  //
  // If the returned status is kOk:
  // - Non-EOS (end of stream) frame contains decoded video data.
  // - EOS frame indicates the end of the stream.
  // Otherwise the returned frame must be NULL.
  typedef base::Callback<void(Status,
                              const scoped_refptr<VideoFrame>&)> DecodeCB;
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) = 0;

  // Some VideoDecoders may queue up multiple VideoFrames from a single
  // DecoderBuffer, if we have any such queued frames this will return the next
  // one. Otherwise we return a NULL VideoFrame.
  virtual scoped_refptr<VideoFrame> GetDecodeOutput();

  // Resets decoder state, fulfilling all pending DecodeCB and dropping extra
  // queued decoded data. After this call, the decoder is back to an initialized
  // clean state.
  // Note: No VideoDecoder calls should be made before |closure| is executed.
  virtual void Reset(const base::Closure& closure) = 0;

  // Stops decoder, fires any pending callbacks and sets the decoder to an
  // uninitialized state. A VideoDecoder cannot be re-initialized after it has
  // been stopped.
  // Note that if Initialize() is pending or has finished successfully, Stop()
  // must be called before destructing the decoder.
  virtual void Stop() = 0;

  // Returns true if the decoder needs bitstream conversion before decoding.
  virtual bool NeedsBitstreamConversion() const;

  // Returns true if the decoder currently has the ability to decode and return
  // a VideoFrame. Most implementations can allocate a new VideoFrame and hence
  // this will always return true. Override and return false for decoders that
  // use a fixed set of VideoFrames for decoding.
  virtual bool CanReadWithoutStalling() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDecoder);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_DECODER_H_
