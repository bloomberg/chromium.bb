// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_
#define MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/android/demuxer_stream_player_params.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class MediaCodecBridge;

// Class for managing all the decoding tasks. Each decoding task will be posted
// onto the same thread. The thread will be stopped once Stop() is called.
class MediaDecoderJob {
 public:
  enum DecodeStatus {
    DECODE_SUCCEEDED,
    DECODE_TRY_ENQUEUE_INPUT_AGAIN_LATER,
    DECODE_TRY_DEQUEUE_OUTPUT_AGAIN_LATER,
    DECODE_FORMAT_CHANGED,
    DECODE_INPUT_END_OF_STREAM,
    DECODE_OUTPUT_END_OF_STREAM,
    DECODE_FAILED,
  };

  struct Deleter {
    inline void operator()(MediaDecoderJob* ptr) const { ptr->Release(); }
  };

  virtual ~MediaDecoderJob();

  // Callback when a decoder job finishes its work. Args: whether decode
  // finished successfully, presentation time, audio output bytes.
  typedef base::Callback<void(DecodeStatus, const base::TimeDelta&,
                              size_t)> DecoderCallback;

  // Called by MediaSourcePlayer to decode some data.
  void Decode(const AccessUnit& unit,
              const base::TimeTicks& start_time_ticks,
              const base::TimeDelta& start_presentation_timestamp,
              const MediaDecoderJob::DecoderCallback& callback);

  // Flush the decoder.
  void Flush();

  // Called on the UI thread to indicate that one decode cycle has completed.
  void OnDecodeCompleted();

  bool is_decoding() const { return is_decoding_; }

 protected:
  MediaDecoderJob(const scoped_refptr<base::MessageLoopProxy>& decoder_loop,
                  MediaCodecBridge* media_codec_bridge);

  // Release the output buffer and render it.
  virtual void ReleaseOutputBuffer(
      int outputBufferIndex, size_t size,
      const base::TimeDelta& presentation_timestamp,
      const MediaDecoderJob::DecoderCallback& callback,
      DecodeStatus status) = 0;

  // Returns true if the "time to render" needs to be computed for frames in
  // this decoder job.
  virtual bool ComputeTimeToRender() const = 0;

 private:
  // Causes this instance to be deleted on the thread it is bound to.
  void Release();

  DecodeStatus QueueInputBuffer(const AccessUnit& unit);

  // Helper function to decoder data on |thread_|. |unit| contains all the data
  // to be decoded. |start_time_ticks| and |start_presentation_timestamp|
  // represent the system time and the presentation timestamp when the first
  // frame is rendered. We use these information to estimate when the current
  // frame should be rendered. If |needs_flush| is true, codec needs to be
  // flushed at the beginning of this call.
  void DecodeInternal(const AccessUnit& unit,
                      const base::TimeTicks& start_time_ticks,
                      const base::TimeDelta& start_presentation_timestamp,
                      bool needs_flush,
                      const MediaDecoderJob::DecoderCallback& callback);

  // The UI message loop where callbacks should be dispatched.
  scoped_refptr<base::MessageLoopProxy> ui_loop_;

  // The message loop that decoder job runs on.
  scoped_refptr<base::MessageLoopProxy> decoder_loop_;

  // The media codec bridge used for decoding. Owned by derived class.
  // NOTE: This MUST NOT be accessed in the destructor.
  MediaCodecBridge* media_codec_bridge_;

  // Whether the decoder needs to be flushed.
  bool needs_flush_;

  // Whether input EOS is encountered.
  bool input_eos_encountered_;

  // Weak pointer passed to media decoder jobs for callbacks. It is bounded to
  // the decoder thread.
  base::WeakPtrFactory<MediaDecoderJob> weak_this_;

  // Whether the decoder is actively decoding data.
  bool is_decoding_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MediaDecoderJob);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_
