// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_VIDEO_DECODER_JOB_H_
#define MEDIA_BASE_ANDROID_VIDEO_DECODER_JOB_H_

#include <jni.h>

#include "media/base/android/media_decoder_job.h"

namespace media {

class VideoCodecBridge;

// Class for managing video decoding jobs.
class VideoDecoderJob : public MediaDecoderJob {
 public:
  virtual ~VideoDecoderJob();

  // Create a new VideoDecoderJob instance.
  // |video_codec| - The video format the object needs to decode.
  // |is_secure| - Whether secure decoding is required.
  // |size| -  The natural size of the output frames.
  // |surface| - The surface to render the frames to.
  // |media_crypto| - Handle to a Java object responsible for decrypting the
  // video data.
  // |request_data_cb| - Callback used to request more data for the decoder.
  static VideoDecoderJob* Create(const VideoCodec video_codec,
                                 bool is_secure,
                                 const gfx::Size& size,
                                 jobject surface,
                                 jobject media_crypto,
                                 const base::Closure& request_data_cb);

 private:
  VideoDecoderJob(scoped_ptr<VideoCodecBridge> video_codec_bridge,
                  const base::Closure& request_data_cb);

  // MediaDecoderJob implementation.
  virtual void ReleaseOutputBuffer(
      int output_buffer_index,
      size_t size,
      bool render_output,
      const ReleaseOutputCompletionCallback& callback) OVERRIDE;

  virtual bool ComputeTimeToRender() const OVERRIDE;

  scoped_ptr<VideoCodecBridge> video_codec_bridge_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_VIDEO_DECODER_JOB_H_
