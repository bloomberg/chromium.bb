// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_DECODER_CLIENT_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_DECODER_CLIENT_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "media/gpu/test/video_player/video_player.h"
#include "media/video/video_decode_accelerator.h"

namespace media {

class GpuVideoDecodeAcceleratorFactory;

namespace test {

class EncodedDataHelper;
class FrameRenderer;

// The video decoder client is responsible for the communication between the
// video player and the video decoder. It also communicates with the frame
// renderer and other components. The video decoder client can only have one
// active decoder at any time. To decode a different stream the DestroyDecoder()
// and CreateDecoder() functions have to be called to destroy and re-create the
// decoder.
//
// All communication with the decoder is done on the |decoder_client_thread_|,
// so callbacks scheduled by the decoder can be executed asynchronously. This is
// necessary if we don't want to interrupt the test flow.
class VideoDecoderClient : public VideoDecodeAccelerator::Client {
 public:
  ~VideoDecoderClient() override;

  // Return an instance of the VideoDecoderClient. The |frame_renderer| will not
  // be owned by the decoder client, the caller should guarantee it exists for
  // the entire lifetime of the decoder client. The |event_cb| will be called
  // whenever an event occurs (e.g. frame decoded) and should be thread-safe.
  static std::unique_ptr<VideoDecoderClient> Create(
      const VideoPlayer::EventCallback& event_cb,
      FrameRenderer* frame_renderer);

  // Create a decoder with specified |config|, video |stream| and video
  // |stream_size|. The video stream will not be owned by the decoder client,
  // the caller should guarantee it exists until DestroyDecoder() is called.
  void CreateDecoder(const VideoDecodeAccelerator::Config& config,
                     const std::vector<uint8_t>& stream);
  // Destroy the currently active decoder.
  void DestroyDecoder();

  // Queue the next video stream fragment to be decoded.
  void DecodeNextFragment();

 private:
  VideoDecoderClient(const VideoPlayer::EventCallback& event_cb,
                     FrameRenderer* renderer);

  bool Initialize();
  void Destroy();

  // VideoDecodeAccelerator::Client implementation
  void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                             VideoPixelFormat pixel_format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& size,
                             uint32_t texture_target) override;
  void DismissPictureBuffer(int32_t picture_buffer_id) override;
  void PictureReady(const Picture& picture) override;
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override;
  void NotifyFlushDone() override;
  void NotifyResetDone() override;
  void NotifyError(VideoDecodeAccelerator::Error error) override;

  void CreateDecoderFactoryTask(base::WaitableEvent* done);
  void CreateDecoderTask(VideoDecodeAccelerator::Config config,
                         const std::vector<uint8_t>* stream,
                         base::WaitableEvent* done);
  void DestroyDecoderTask(base::WaitableEvent* done);
  void DecodeNextFragmentTask();

  // Called by the renderer in response to a CreatePictureBuffers request.
  void OnPictureBuffersCreatedTask(std::vector<PictureBuffer> buffers);
  // Called by the renderer in response to a RenderPicture request.
  void OnPictureRenderedTask(int32_t picture_buffer_id);

  // Get the next bitstream buffer id to be used.
  int32_t GetNextBitstreamBufferId();

  VideoPlayer::EventCallback event_cb_;
  FrameRenderer* const frame_renderer_;

  std::unique_ptr<GpuVideoDecodeAcceleratorFactory> decoder_factory_;
  std::unique_ptr<VideoDecodeAccelerator> decoder_;
  base::Thread decoder_client_thread_;

  int32_t next_bitstream_buffer_id_ = 0;
  // TODO(dstaessens@) Replace with StreamParser.
  std::unique_ptr<media::test::EncodedDataHelper> encoded_data_helper_;

  SEQUENCE_CHECKER(video_player_sequence_checker_);
  SEQUENCE_CHECKER(decoder_client_sequence_checker_);

  base::WeakPtr<VideoDecoderClient> weak_this_;
  base::WeakPtrFactory<VideoDecoderClient> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderClient);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_DECODER_CLIENT_H_
