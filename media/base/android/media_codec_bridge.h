// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/size.h"

namespace media {

// This class serves as a bridge for native code to call java functions inside
// Android MediaCodec class. For more information on Android MediaCodec, check
// http://developer.android.com/reference/android/media/MediaCodec.html
// Note: MediaCodec is only available on JB and greater.
// Use AudioCodecBridge or VideoCodecBridge to create an instance of this
// object.
class MEDIA_EXPORT MediaCodecBridge {
 public:
  enum DequeueBufferInfo {
    INFO_OUTPUT_BUFFERS_CHANGED = -3,
    INFO_OUTPUT_FORMAT_CHANGED = -2,
    INFO_TRY_AGAIN_LATER = -1,
    INFO_MEDIA_CODEC_ERROR = -1000,
  };

  static const base::TimeDelta kTimeOutInfinity;
  static const base::TimeDelta kTimeOutNoWait;

  // Returns true if MediaCodec is available on the device.
  static bool IsAvailable();

  virtual ~MediaCodecBridge();

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
      base::TimeDelta timeout, size_t* offset, size_t* size,
      base::TimeDelta* presentation_time, bool* end_of_stream);

  // Returns the buffer to the codec. If you previously specified a surface
  // when configuring this video decoder you can optionally render the buffer.
  void ReleaseOutputBuffer(int index, bool render);

  // Gets output buffers from media codec and keeps them inside the java class.
  // To access them, use DequeueOutputBuffer().
  void GetOutputBuffers();

  static bool RegisterMediaCodecBridge(JNIEnv* env);

 protected:
  explicit MediaCodecBridge(const char* mime);

  // Calls start() against the media codec instance. Used in StartXXX() after
  // configuring media codec.
  void StartInternal();

  jobject media_codec() { return j_media_codec_.obj(); }

 private:
  // Java MediaCodec instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_codec_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecBridge);
};

class AudioCodecBridge : public MediaCodecBridge {
 public:
  // Returns an AudioCodecBridge instance if |codec| is supported, or a NULL
  // pointer otherwise.
  static AudioCodecBridge* Create(const AudioCodec codec);

  // Start the audio codec bridge.
  bool Start(const AudioCodec codec, int sample_rate, int channel_count,
             const uint8* extra_data, size_t extra_data_size,
             bool play_audio, jobject media_crypto);

  // Play the output buffer. This call must be called after
  // DequeueOutputBuffer() and before ReleaseOutputBuffer.
  void PlayOutputBuffer(int index, size_t size);

 private:
  explicit AudioCodecBridge(const char* mime);

  // Configure the java MediaFormat object with the extra codec data passed in.
  bool ConfigureMediaFormat(jobject j_format, const AudioCodec codec,
                            const uint8* extra_data, size_t extra_data_size);
};

class VideoCodecBridge : public MediaCodecBridge {
 public:
  // Returns an VideoCodecBridge instance if |codec| is supported, or a NULL
  // pointer otherwise.
  static VideoCodecBridge* Create(const VideoCodec codec);

  // Start the video codec bridge.
  // TODO(qinmin): Pass codec specific data if available.
  bool Start(const VideoCodec codec, const gfx::Size& size, jobject surface,
             jobject media_crypto);

 private:
  explicit VideoCodecBridge(const char* mime);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_H_

