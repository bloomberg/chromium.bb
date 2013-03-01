// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/time.h"
#include "ui/gfx/size.h"

namespace media {

// This class serves as a bridge for native code to call java functions inside
// Android MediaCodec class. For more information on Android MediaCodec, check
// http://developer.android.com/reference/android/media/MediaCodec.html
// Note: MediaCodec is only available on JB and greater.
class MediaCodecBridge {
 public:
  enum Codec {
    AUDIO_MPEG,
    VIDEO_H264,
    VIDEO_VP8,
  };

  enum DequeueBufferInfo {
    INFO_OUTPUT_BUFFERS_CHANGED = -3,
    INFO_OUTPUT_FORMAT_CHANGED = -2,
    INFO_TRY_AGAIN_LATER = -1,
  };

  static const base::TimeDelta kTimeOutInfinity;
  static const base::TimeDelta kTimeOutNoWait;

  // Returns true if MediaCodec is available on the device.
  static bool IsAvailable();

  explicit MediaCodecBridge(const Codec codec);

  ~MediaCodecBridge();

  // Starts decoding with the given codec params.
  void StartAudio(
      const Codec codec, int sample_rate, int channel_count);
  void StartVideo(
      const Codec codec, const gfx::Size& size, jobject surface);

  // Resets both input and output, all indices previously returned in calls to
  // DequeueInputBuffer() and DequeueOutputBuffer() become invalid.
  // Please note that this clears all the inputs in the media codec. In other
  // words, there will be no outputs until new input is provided.
  void Reset();

  // Finishes the decode/encode session. The instance remains active
  // and ready to be StartAudio/Video()ed again. HOWEVER, due to the buggy
  // vendor's implementation , b/8125974, Stop() -> StartAudio/Video() may not
  // work on some devices. For reliability, Stop() -> delete and recreate new
  // instance -> StartAudio/Video() is recommended.
  void Stop();

  // Used for getting output format. This is valid after DequeueInputBuffer()
  // returns a format change by returning INFO_OUTPUT_FORMAT_CHANGED
  void GetOutputFormat(int* width, int* height);

  // Submits a byte array to the given input buffer. Call this after getting an
  // available buffer from DequeueInputBuffer(). Returns the number of bytes
  // put to the input buffer.
  size_t QueueInputBuffer(int index, const uint8* data, int size,
                          const base::TimeDelta& presentation_time);

  // Submits an empty buffer with a EOS (END OF STREAM) flag.
  void QueueEOS(int input_buffer_index);

  // Returns the index of an input buffer to be filled with valid data or
  // INFO_TRY_AGAIN_LATER if no such buffer is currently available.
  // Use kTimeOutInfinity for infinite timeout.
  int DequeueInputBuffer(base::TimeDelta timeout);

  // Dequeues an output buffer, block at most timeout_us microseconds.
  // Returns the index of an output buffer that has been successfully decoded
  // or one of DequeueBufferInfo above.
  // Use kTimeOutInfinity for infinite timeout.
  int DequeueOutputBuffer(
      base::TimeDelta timeout, int* offset, int* size,
      base::TimeDelta* presentation_time, bool* end_of_stream);

  // Returns the buffer to the codec. If you previously specified a surface
  // when configuring this video decoder you can optionally render the buffer.
  void ReleaseOutputBuffer(int index, bool render);

  // Gets output buffers from media codec and keeps them inside this class.
  // To access them, use DequeueOutputBuffer() and GetFromOutputBuffer().
  int GetOutputBuffers();

 private:

  // Calls start() against the media codec instance. Used in StartXXX() after
  // configuring media codec.
  void Start();

  // Gets input buffers from media codec and keeps them inside this class.
  // To access them, use DequeueInputBuffer(), PutToInputBuffer() and
  // QueueInputBuffer().
  int GetInputBuffers();

  // Java MediaCodec instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_codec_;

  // Input buffers used for *InputBuffer() methods.
  base::android::ScopedJavaGlobalRef<jobjectArray> j_input_buffers_;

  // Output buffers used for *InputBuffer() methods.
  base::android::ScopedJavaGlobalRef<jobjectArray> j_output_buffers_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_

