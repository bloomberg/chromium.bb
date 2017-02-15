// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_IMPL_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_codec_direction.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// A bridge to a Java MediaCodec.
class MEDIA_EXPORT MediaCodecBridgeImpl : public MediaCodecBridge {
 public:
  ~MediaCodecBridgeImpl() override;

  // Calls start() against the media codec instance. Returns whether media
  // codec was successfully started.
  bool Start() override;

  // Finishes the decode/encode session. The instance remains active
  // and ready to be StartAudio/Video()ed again. HOWEVER, due to the buggy
  // vendor's implementation , b/8125974, Stop() -> StartAudio/Video() may not
  // work on some devices. For reliability, Stop() -> delete and recreate new
  // instance -> StartAudio/Video() is recommended.
  void Stop() override;

  // Calls flush() on the MediaCodec. All indices previously returned in calls
  // to DequeueInputBuffer() and DequeueOutputBuffer() become invalid. Please
  // note that this clears all the inputs in the media codec. In other words,
  // there will be no outputs until new input is provided. Returns
  // MEDIA_CODEC_ERROR if an unexpected error happens, or MEDIA_CODEC_OK
  // otherwise.
  MediaCodecStatus Flush() override;

  // Used for getting the output size. This is valid after DequeueInputBuffer()
  // returns a format change by returning INFO_OUTPUT_FORMAT_CHANGED.
  // Returns MEDIA_CODEC_ERROR if an error occurs, or MEDIA_CODEC_OK otherwise.
  MediaCodecStatus GetOutputSize(gfx::Size* size) override;

  // Used for checking for new sampling rate after DequeueInputBuffer() returns
  // INFO_OUTPUT_FORMAT_CHANGED
  // Returns MEDIA_CODEC_ERROR if an error occurs, or MEDIA_CODEC_OK otherwise.
  MediaCodecStatus GetOutputSamplingRate(int* sampling_rate) override;

  // Fills |channel_count| with the number of audio channels. Useful after
  // INFO_OUTPUT_FORMAT_CHANGED.
  // Returns MEDIA_CODEC_ERROR if an error occurs, or MEDIA_CODEC_OK otherwise.
  MediaCodecStatus GetOutputChannelCount(int* channel_count) override;

  // Submits a byte array to the given input buffer. Call this after getting an
  // available buffer from DequeueInputBuffer(). If |data| is NULL, assume the
  // input buffer has already been populated (but still obey |size|).
  // |data_size| must be less than kint32max (because Java).
  MediaCodecStatus QueueInputBuffer(int index,
                                    const uint8_t* data,
                                    size_t data_size,
                                    base::TimeDelta presentation_time) override;

  // As above but for encrypted buffers. NULL |subsamples| indicates the
  // whole buffer is encrypted.
  MediaCodecStatus QueueSecureInputBuffer(
      int index,
      const uint8_t* data,
      size_t data_size,
      const std::string& key_id,
      const std::string& iv,
      const std::vector<SubsampleEntry>& subsamples,
      const EncryptionScheme& encryption_scheme,
      base::TimeDelta presentation_time) override;

  // Submits an empty buffer with the END_OF_STREAM flag set.
  void QueueEOS(int input_buffer_index) override;

  // Returns:
  // MEDIA_CODEC_OK if an input buffer is ready to be filled with valid data,
  // MEDIA_CODEC_ENQUEUE_INPUT_AGAIN_LATER if no such buffer is available, or
  // MEDIA_CODEC_ERROR if unexpected error happens.
  MediaCodecStatus DequeueInputBuffer(base::TimeDelta timeout,
                                      int* index) override;

  // Dequeues an output buffer, block for up to |timeout|.
  // Returns the status of this operation. If OK is returned, the output
  // parameters should be populated. Otherwise, the values of output parameters
  // should not be used.  Output parameters other than index/offset/size are
  // optional and only set if not NULL.
  MediaCodecStatus DequeueOutputBuffer(base::TimeDelta timeout,
                                       int* index,
                                       size_t* offset,
                                       size_t* size,
                                       base::TimeDelta* presentation_time,
                                       bool* end_of_stream,
                                       bool* key_frame) override;

  // Returns the buffer to the codec. If you previously specified a surface when
  // configuring this video decoder you can optionally render the buffer.
  void ReleaseOutputBuffer(int index, bool render) override;

  // Returns an input buffer's base pointer and capacity.
  MediaCodecStatus GetInputBuffer(int input_buffer_index,
                                  uint8_t** data,
                                  size_t* capacity) override;

  // Copies |num| bytes from output buffer |index|'s |offset| into the memory
  // region pointed to by |dst|. To avoid overflows, the size of both source
  // and destination must be at least |num| bytes, and should not overlap.
  // Returns MEDIA_CODEC_ERROR if an error occurs, or MEDIA_CODEC_OK otherwise.
  MediaCodecStatus CopyFromOutputBuffer(int index,
                                        size_t offset,
                                        void* dst,
                                        size_t num) override;

  // Gets the component name. Before API level 18 this returns an empty string.
  std::string GetName() override;

 protected:
  MediaCodecBridgeImpl(const std::string& mime,
                       bool is_secure,
                       MediaCodecDirection direction,
                       bool require_software_codec);

  jobject media_codec() { return j_media_codec_.obj(); }

  MediaCodecDirection direction_;

 private:
  // Fills the given input buffer. Returns false if |data_size| exceeds the
  // input buffer's capacity (and doesn't touch the input buffer in that case).
  bool FillInputBuffer(int index,
                       const uint8_t* data,
                       size_t data_size) WARN_UNUSED_RESULT;

  // Gets the address of the data in the given output buffer given by |index|
  // and |offset|. The number of bytes available to read is written to
  // |*capacity| and the address is written to |*addr|. Returns
  // MEDIA_CODEC_ERROR if an error occurs, or MEDIA_CODEC_OK otherwise.
  MediaCodecStatus GetOutputBufferAddress(int index,
                                          size_t offset,
                                          const uint8_t** addr,
                                          size_t* capacity);

  // The Java MediaCodecBridge instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_codec_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecBridgeImpl);
};

// A MediaCodecBridge for audio decoding.
// TODO(watk): Move this into MediaCodecBridgeImpl.
class MEDIA_EXPORT AudioCodecBridge : public MediaCodecBridgeImpl {
 public:
  // See MediaCodecUtil::IsKnownUnaccelerated().
  static bool IsKnownUnaccelerated(const AudioCodec& codec);

  // Returns an AudioCodecBridge instance if |codec| is supported, or a NULL
  // pointer otherwise.
  static AudioCodecBridge* Create(const AudioCodec& codec);

  // Starts the audio codec bridge.
  bool ConfigureAndStart(const AudioDecoderConfig& config,
                         jobject media_crypto) WARN_UNUSED_RESULT;

  bool ConfigureAndStart(const AudioCodec& codec,
                         int sample_rate,
                         int channel_count,
                         const uint8_t* extra_data,
                         size_t extra_data_size,
                         int64_t codec_delay_ns,
                         int64_t seek_preroll_ns,
                         jobject media_crypto) WARN_UNUSED_RESULT;

 private:
  explicit AudioCodecBridge(const std::string& mime);

  // Configure the java MediaFormat object with the extra codec data passed in.
  bool ConfigureMediaFormat(jobject j_format,
                            const AudioCodec& codec,
                            const uint8_t* extra_data,
                            size_t extra_data_size,
                            int64_t codec_delay_ns,
                            int64_t seek_preroll_ns);
};

// A MediaCodecBridge for video encoding and decoding.
// TODO(watk): Move this into MediaCodecBridgeImpl.
class MEDIA_EXPORT VideoCodecBridge : public MediaCodecBridgeImpl {
 public:
  // See MediaCodecUtil::IsKnownUnaccelerated().
  static bool IsKnownUnaccelerated(const VideoCodec& codec,
                                   MediaCodecDirection direction);

  // Create, start, and return a VideoCodecBridge decoder or NULL on failure.
  static VideoCodecBridge* CreateDecoder(
      const VideoCodec& codec,
      bool is_secure,         // Will be used with encrypted content.
      const gfx::Size& size,  // Output frame size.
      jobject surface,        // Output surface, optional.
      jobject media_crypto,   // MediaCrypto object, optional.
      // Codec specific data. See MediaCodec docs.
      const std::vector<uint8_t>& csd0,
      const std::vector<uint8_t>& csd1,
      // Should adaptive playback be allowed if supported.
      bool allow_adaptive_playback = true,
      bool require_software_codec = false);

  // Create, start, and return a VideoCodecBridge encoder or NULL on failure.
  static VideoCodecBridge* CreateEncoder(
      const VideoCodec& codec,  // e.g. media::kCodecVP8
      const gfx::Size& size,    // input frame size
      int bit_rate,             // bits/second
      int frame_rate,           // frames/second
      int i_frame_interval,     // count
      int color_format);        // MediaCodecInfo.CodecCapabilities.

  void SetVideoBitrate(int bps, int frame_rate);
  void RequestKeyFrameSoon();

  // Returns whether adaptive playback is supported for this object given
  // the new size.
  bool IsAdaptivePlaybackSupported(int width, int height);

  // Changes the output surface for the MediaCodec. May only be used on API
  // level 23 and higher (Marshmallow).
  bool SetSurface(jobject surface);

  // Test-only method to set the return value of IsAdaptivePlaybackSupported().
  // Without this function, the return value of that function will be device
  // dependent. If |adaptive_playback_supported| is equal to 0, the return value
  // will be false. If |adaptive_playback_supported| is larger than 0, the
  // return value will be true.
  void set_adaptive_playback_supported_for_testing(
      int adaptive_playback_supported) {
    adaptive_playback_supported_for_testing_ = adaptive_playback_supported;
  }

 private:
  VideoCodecBridge(const std::string& mime,
                   bool is_secure,
                   MediaCodecDirection direction,
                   bool require_software_codec);

  int adaptive_playback_supported_for_testing_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_BRIDGE_IMPL_H_
