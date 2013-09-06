// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_
#define MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/android/media_codec_bridge.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

// Class for managing all the decoding tasks. Each decoding task will be posted
// onto the same thread. The thread will be stopped once Stop() is called.
class MediaDecoderJob {
 public:
  struct Deleter {
    inline void operator()(MediaDecoderJob* ptr) const { ptr->Release(); }
  };

  // Callback when a decoder job finishes its work. Args: whether decode
  // finished successfully, presentation time, audio output bytes.
  typedef base::Callback<void(MediaCodecStatus, const base::TimeDelta&,
                              size_t)> DecoderCallback;

  virtual ~MediaDecoderJob();

  // Called by MediaSourcePlayer when more data for this object has arrived.
  void OnDataReceived(const DemuxerData& data);

  // Prefetch so we know the decoder job has data when we call Decode().
  // |prefetch_cb| - Run when prefetching has completed.
  void Prefetch(const base::Closure& prefetch_cb);

  // Called by MediaSourcePlayer to decode some data.
  // |callback| - Run when decode operation has completed.
  //
  // Returns true if the next decode was started and |callback| will be
  // called when the decode operation is complete.
  // Returns false if a config change is needed. |callback| is ignored
  // and will not be called.
  bool Decode(const base::TimeTicks& start_time_ticks,
              const base::TimeDelta& start_presentation_timestamp,
              const DecoderCallback& callback);

  // Called to stop the last Decode() early.
  // If the decoder is in the process of decoding the next frame, then
  // this method will just allow the decode to complete as normal. If
  // this object is waiting for a data request to complete, then this method
  // will wait for the data to arrive and then call the |callback|
  // passed to Decode() with a status of MEDIA_CODEC_STOPPED. This ensures that
  // the |callback| passed to Decode() is always called and the status
  // reflects whether data was actually decoded or the decode terminated early.
  void StopDecode();

  // Flush the decoder.
  void Flush();

  bool is_decoding() const { return !decode_cb_.is_null(); }

 protected:
  MediaDecoderJob(const scoped_refptr<base::MessageLoopProxy>& decoder_loop,
                  MediaCodecBridge* media_codec_bridge,
                  const base::Closure& request_data_cb);

  // Release the output buffer and render it.
  virtual void ReleaseOutputBuffer(
      int outputBufferIndex, size_t size,
      const base::TimeDelta& presentation_timestamp,
      const DecoderCallback& callback,
      MediaCodecStatus status) = 0;

  // Returns true if the "time to render" needs to be computed for frames in
  // this decoder job.
  virtual bool ComputeTimeToRender() const = 0;

 private:
  // Causes this instance to be deleted on the thread it is bound to.
  void Release();

  MediaCodecStatus QueueInputBuffer(const AccessUnit& unit);

  // Returns true if this object has data to decode.
  bool HasData() const;

  // Initiates a request for more data.
  // |done_cb| is called when more data is available in |received_data_|.
  void RequestData(const base::Closure& done_cb);

  // Posts a task to start decoding the next access unit in |received_data_|.
  void DecodeNextAccessUnit(
      const base::TimeTicks& start_time_ticks,
      const base::TimeDelta& start_presentation_timestamp);

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
                      const DecoderCallback& callback);

  // Called on the UI thread to indicate that one decode cycle has completed.
  void OnDecodeCompleted(MediaCodecStatus status,
                         const base::TimeDelta& presentation_timestamp,
                         size_t audio_output_bytes);

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

  // Callback used to request more data.
  base::Closure request_data_cb_;

  // Callback to run when new data has been received.
  base::Closure on_data_received_cb_;

  // Callback to run when the current Decode() operation completes.
  DecoderCallback decode_cb_;

  // The current access unit being processed.
  size_t access_unit_index_;

  // Data received over IPC from last RequestData() operation.
  DemuxerData received_data_;

  // The index of input buffer that can be used by QueueInputBuffer().
  // If the index is uninitialized or invalid, it must be -1.
  int input_buf_index_;

  bool stop_decode_pending_;

  // Indicates that this object should be destroyed once the current
  // Decode() has completed. This gets set when Release() gets called
  // while there is a decode in progress.
  bool destroy_pending_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MediaDecoderJob);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DECODER_JOB_H_
