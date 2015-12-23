// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/media_export.h"

namespace media {

struct SubsampleEntry;

// These must be in sync with MediaCodecBridge.MEDIA_CODEC_XXX constants in
// MediaCodecBridge.java.
enum MediaCodecStatus {
  MEDIA_CODEC_OK,
  MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER,
  MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER,
  MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED,
  MEDIA_CODEC_OUTPUT_FORMAT_CHANGED,
  MEDIA_CODEC_INPUT_END_OF_STREAM,
  MEDIA_CODEC_OUTPUT_END_OF_STREAM,
  MEDIA_CODEC_NO_KEY,
  MEDIA_CODEC_ABORT,
  MEDIA_CODEC_ERROR
};

// Interface for wrapping different Android MediaCodec implementations. For
// more information on Android MediaCodec, check
// http://developer.android.com/reference/android/media/MediaCodec.html
// Note: MediaCodec is only available on JB and greater.
class MEDIA_EXPORT MediaCodecBridge {
 public:
  virtual ~MediaCodecBridge();

  // Resets both input and output, all indices previously returned in calls to
  // DequeueInputBuffer() and DequeueOutputBuffer() become invalid.
  // Please note that this clears all the inputs in the media codec. In other
  // words, there will be no outputs until new input is provided.
  // Returns MEDIA_CODEC_ERROR if an unexpected error happens, or Media_CODEC_OK
  // otherwise.
  virtual MediaCodecStatus Reset() = 0;

  // Calls start() against the media codec instance. Returns whether media
  // codec was successfully started.
  virtual bool Start() = 0;

  // Finishes the decode/encode session. The instance remains active
  // and ready to be StartAudio/Video()ed again. HOWEVER, due to the buggy
  // vendor's implementation , b/8125974, Stop() -> StartAudio/Video() may not
  // work on some devices. For reliability, Stop() -> delete and recreate new
  // instance -> StartAudio/Video() is recommended.
  virtual void Stop() = 0;

  // Used for getting output format. This is valid after DequeueInputBuffer()
  // returns a format change by returning INFO_OUTPUT_FORMAT_CHANGED
  virtual void GetOutputFormat(int* width, int* height) = 0;

  // Used for checking for new sampling rate after DequeueInputBuffer() returns
  // INFO_OUTPUT_FORMAT_CHANGED
  virtual int GetOutputSamplingRate() = 0;

  // Submits a byte array to the given input buffer. Call this after getting an
  // available buffer from DequeueInputBuffer(). If |data| is NULL, assume the
  // input buffer has already been populated (but still obey |size|).
  // |data_size| must be less than kint32max (because Java).
  virtual MediaCodecStatus QueueInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      const base::TimeDelta& presentation_time) = 0;

  // Similar to the above call, but submits a buffer that is encrypted.  Note:
  // NULL |subsamples| indicates the whole buffer is encrypted.  If |data| is
  // NULL, assume the input buffer has already been populated (but still obey
  // |data_size|).  |data_size| must be less than kint32max (because Java).
  MediaCodecStatus QueueSecureInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      const std::string& key_id,
      const std::string& iv,
      const std::vector<SubsampleEntry>& subsamples,
      const base::TimeDelta& presentation_time);

  // Same QueueSecureInputBuffer overriden for the use with MediaSourcePlayer
  // and MediaCodecPlayer.
  // TODO(timav): remove this method and keep only the one above after we
  // switch to the Spitzer pipeline.
  virtual MediaCodecStatus QueueSecureInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      const std::vector<char>& key_id,
      const std::vector<char>& iv,
      const SubsampleEntry* subsamples,
      int subsamples_size,
      const base::TimeDelta& presentation_time) = 0;

  // Submits an empty buffer with a EOS (END OF STREAM) flag.
  virtual void QueueEOS(int input_buffer_index) = 0;

  // Returns:
  // MEDIA_CODEC_OK if an input buffer is ready to be filled with valid data,
  // MEDIA_CODEC_ENQUEUE_INPUT_AGAIN_LATER if no such buffer is available, or
  // MEDIA_CODEC_ERROR if unexpected error happens.
  // Note: Never use infinite timeout as this would block the decoder thread and
  // prevent the decoder job from being released.
  virtual MediaCodecStatus DequeueInputBuffer(const base::TimeDelta& timeout,
                                              int* index) = 0;

  // Dequeues an output buffer, block at most timeout_us microseconds.
  // Returns the status of this operation. If OK is returned, the output
  // parameters should be populated. Otherwise, the values of output parameters
  // should not be used.  Output parameters other than index/offset/size are
  // optional and only set if not NULL.
  // Note: Never use infinite timeout as this would block the decoder thread and
  // prevent the decoder job from being released.
  // TODO(xhwang): Can we drop |end_of_stream| and return
  // MEDIA_CODEC_OUTPUT_END_OF_STREAM?
  virtual MediaCodecStatus DequeueOutputBuffer(
      const base::TimeDelta& timeout,
      int* index,
      size_t* offset,
      size_t* size,
      base::TimeDelta* presentation_time,
      bool* end_of_stream,
      bool* key_frame) = 0;

  // Returns the buffer to the codec. If you previously specified a surface when
  // configuring this video decoder you can optionally render the buffer.
  virtual void ReleaseOutputBuffer(int index, bool render) = 0;

  // Returns the number of output buffers used by the codec.
  // TODO(qinmin): this call is deprecated in Lollipop.
  virtual int GetOutputBuffersCount();

  // Returns the capacity of each output buffer used by the codec.
  // TODO(qinmin): this call is deprecated in Lollipop.
  virtual size_t GetOutputBuffersCapacity();

  // Returns an input buffer's base pointer and capacity.
  virtual void GetInputBuffer(int input_buffer_index,
                              uint8_t** data,
                              size_t* capacity) = 0;

  // Copy |dst_size| bytes from output buffer |index|'s |offset| onwards into
  // |*dst|.
  virtual bool CopyFromOutputBuffer(int index,
                                    size_t offset,
                                    void* dst,
                                    int dst_size) = 0;

 protected:
  MediaCodecBridge();

  // Fills a particular input buffer; returns false if |data_size| exceeds the
  // input buffer's capacity (and doesn't touch the input buffer in that case).
  bool FillInputBuffer(int index,
                       const uint8_t* data,
                       size_t data_size) WARN_UNUSED_RESULT;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_
