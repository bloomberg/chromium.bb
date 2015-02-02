// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_EXTERNAL_VIDEO_ENCODER_H_
#define MEDIA_CAST_SENDER_EXTERNAL_VIDEO_ENCODER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/sender/video_encoder.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace cast {

// Cast MAIN thread proxy to the internal media::VideoEncodeAccelerator
// implementation running on a separate thread.  Encodes media::VideoFrames and
// emits media::cast::EncodedFrames.
class ExternalVideoEncoder : public VideoEncoder {
 public:
  ExternalVideoEncoder(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const VideoSenderConfig& video_config,
      const gfx::Size& frame_size,
      const CastInitializationCallback& initialization_cb,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      const CreateVideoEncodeMemoryCallback& create_video_encode_memory_cb);

  ~ExternalVideoEncoder() override;

  // VideoEncoder implementation.
  bool CanEncodeVariedFrameSizes() const override;
  bool EncodeVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& reference_time,
      const FrameEncodedCallback& frame_encoded_callback) override;
  void SetBitRate(int new_bit_rate) override;
  void GenerateKeyFrame() override;
  void LatestFrameIdToReference(uint32 frame_id) override;

 private:
  class VEAClientImpl;

  // Method invoked by the CreateVideoEncodeAcceleratorCallback to construct a
  // VEAClientImpl to own and interface with a new |vea|.  Upon return,
  // |client_| holds a reference to the new VEAClientImpl.
  void OnCreateVideoEncodeAccelerator(
      const gfx::Size& frame_size,
      VideoCodecProfile codec_profile,
      int max_frame_rate,
      const CastInitializationCallback& initialization_cb,
      scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner,
      scoped_ptr<media::VideoEncodeAccelerator> vea);

  const scoped_refptr<CastEnvironment> cast_environment_;
  const CreateVideoEncodeMemoryCallback create_video_encode_memory_cb_;
  int bit_rate_;
  bool key_frame_requested_;

  scoped_refptr<VEAClientImpl> client_;

  // Provides a weak pointer for the OnCreateVideoEncoderAccelerator() callback.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<ExternalVideoEncoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVideoEncoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_EXTERNAL_VIDEO_ENCODER_H_
