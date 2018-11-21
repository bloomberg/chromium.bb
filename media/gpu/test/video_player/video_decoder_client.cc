// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_decoder_client.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "media/gpu/test/video_player/frame_renderer.h"

#define DVLOGF(level) DVLOG(level) << __func__ << "(): "
#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace media {
namespace test {

VideoDecoderClient::VideoDecoderClient(
    const VideoPlayer::EventCallback& event_cb,
    FrameRenderer* renderer)
    : event_cb_(event_cb),
      frame_renderer_(renderer),
      decoder_client_thread_("VDAClientDecoderThread"),
      weak_this_factory_(this) {
  DETACH_FROM_SEQUENCE(decoder_client_sequence_checker_);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VideoDecoderClient::~VideoDecoderClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);
  Destroy();
}

// static
std::unique_ptr<VideoDecoderClient> VideoDecoderClient::Create(
    const VideoPlayer::EventCallback& event_cb,
    FrameRenderer* frame_renderer) {
  auto decoder_client =
      base::WrapUnique(new VideoDecoderClient(event_cb, frame_renderer));
  if (!decoder_client->Initialize()) {
    return nullptr;
  }
  return decoder_client;
}

bool VideoDecoderClient::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);
  DCHECK(!decoder_client_thread_.IsRunning());
  DCHECK(event_cb_.MaybeValid() && frame_renderer_);

  if (!decoder_client_thread_.Start()) {
    VLOGF(1) << "Failed to start decoder thread";
    return false;
  }

  // Create a decoder factory, which will be used to create decoders.
  base::WaitableEvent done;
  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::CreateDecoderFactoryTask,
                                weak_this_, &done));
  done.Wait();

  return true;
}

void VideoDecoderClient::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  DestroyDecoder();
  decoder_client_thread_.Stop();
}

void VideoDecoderClient::CreateDecoder(
    const VideoDecodeAccelerator::Config& config,
    const std::vector<uint8_t>& stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  base::WaitableEvent done;
  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::CreateDecoderTask,
                                weak_this_, config, &stream, &done));
  done.Wait();
}

void VideoDecoderClient::DestroyDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  base::WaitableEvent done;
  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::DestroyDecoderTask,
                                weak_this_, &done));
  done.Wait();
}

void VideoDecoderClient::DecodeNextFragment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoDecoderClient::DecodeNextFragmentTask, weak_this_));
}

void VideoDecoderClient::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    VideoPixelFormat pixel_format,
    uint32_t textures_per_buffer,
    const gfx::Size& size,
    uint32_t texture_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_NE(pixel_format, PIXEL_FORMAT_UNKNOWN);
  DCHECK_EQ(textures_per_buffer, 1u);
  DVLOGF(4) << "Requested " << requested_num_of_buffers
            << " picture buffers with size " << size.height() << "x"
            << size.height();

  // TODO(dstaessens@) Avoid using Unretained(this) here.
  FrameRenderer::PictureBuffersCreatedCB cb = BindToCurrentLoop(
      base::BindOnce(&VideoDecoderClient::OnPictureBuffersCreatedTask,
                     base::Unretained(this)));
  frame_renderer_->CreatePictureBuffers(requested_num_of_buffers, pixel_format,
                                        size, texture_target, std::move(cb));
}

void VideoDecoderClient::DismissPictureBuffer(int32_t picture_buffer_id) {
  // TODO(dstaessens@) support dismissing picture buffers.
  NOTIMPLEMENTED();
}

void VideoDecoderClient::PictureReady(const Picture& picture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4) << "Picture buffer ID: " << picture.picture_buffer_id();

  event_cb_.Run(VideoPlayerEvent::kFrameDecoded);

  // TODO(dstaessens@) Avoid using Unretained(this) here.
  FrameRenderer::PictureRenderedCB cb = BindToCurrentLoop(
      base::BindOnce(&VideoDecoderClient::OnPictureRenderedTask,
                     base::Unretained(this), picture.picture_buffer_id()));
  frame_renderer_->RenderPicture(picture, std::move(cb));
}

void VideoDecoderClient::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Queue the next fragment to be decoded. Flush when we reached the end of the
  // stream.
  if (encoded_data_helper_->ReachEndOfStream()) {
    decoder_->Flush();
  } else {
    DecodeNextFragmentTask();
  }
}

void VideoDecoderClient::NotifyFlushDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);

  event_cb_.Run(VideoPlayerEvent::kFlushDone);
}

void VideoDecoderClient::NotifyResetDone() {
  NOTIMPLEMENTED();
}

void VideoDecoderClient::NotifyError(VideoDecodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);

  switch (error) {
    case VideoDecodeAccelerator::ILLEGAL_STATE:
      LOG(FATAL) << "ILLEGAL_STATE";
      break;
    case VideoDecodeAccelerator::INVALID_ARGUMENT:
      LOG(FATAL) << "INVALID_ARGUMENT";
      break;
    case VideoDecodeAccelerator::UNREADABLE_INPUT:
      LOG(FATAL) << "UNREADABLE_INPUT";
      break;
    case VideoDecodeAccelerator::PLATFORM_FAILURE:
      LOG(FATAL) << "PLATFORM_FAILURE";
      break;
    default:
      LOG(FATAL) << "Unknown error";
      break;
  }
}

void VideoDecoderClient::CreateDecoderFactoryTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  LOG_ASSERT(!decoder_factory_) << "Decoder factory already created";
  DVLOGF(4);

  if (frame_renderer_->GetGLContext()) {
    decoder_factory_ = GpuVideoDecodeAcceleratorFactory::Create(
        base::BindRepeating(&FrameRenderer::GetGLContext,
                            base::Unretained(frame_renderer_)),
        base::BindRepeating([]() { return true; }),
        base::BindRepeating([](uint32_t, uint32_t,
                               const scoped_refptr<gl::GLImage>&,
                               bool) { return true; }));
  } else {
    decoder_factory_ = GpuVideoDecodeAcceleratorFactory::CreateWithNoGL();
  }

  done->Signal();
}

void VideoDecoderClient::CreateDecoderTask(
    VideoDecodeAccelerator::Config config,
    const std::vector<uint8_t>* stream,
    base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  LOG_ASSERT(decoder_factory_) << "Decoder factory not created yet";
  LOG_ASSERT(!decoder_) << "Can't create decoder: already created";
  DVLOGF(4) << "Profile: " << config.profile;

  encoded_data_helper_ =
      std::make_unique<EncodedDataHelper>(*stream, config.profile);

  gpu::GpuDriverBugWorkarounds gpu_driver_bug_workarounds;
  gpu::GpuPreferences gpu_preferences;
  decoder_ = decoder_factory_->CreateVDA(
      this, config, gpu_driver_bug_workarounds, gpu_preferences);

  LOG_ASSERT(decoder_) << "Failed creating a VDA";
  decoder_->TryToSetupDecodeOnSeparateThread(
      weak_this_, base::ThreadTaskRunnerHandle::Get());

  done->Signal();
}

void VideoDecoderClient::DestroyDecoderTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  LOG_ASSERT(decoder_) << "Can't destroy decoder: not created yet";
  DVLOGF(4);

  // Invalidate all scheduled tasks.
  weak_this_factory_.InvalidateWeakPtrs();

  // Destroying a decoder requires an active GL context.
  // TODO(dstaessens@) Investigate making the decoder manage the GL context.
  frame_renderer_->AcquireGLContext();
  decoder_.reset();
  frame_renderer_->ReleaseGLContext();

  done->Signal();
}

void VideoDecoderClient::DecodeNextFragmentTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  if (encoded_data_helper_->ReachEndOfStream()) {
    LOG(ERROR) << "End of stream reached";
    return;
  }

  std::string fragment_bytes = encoded_data_helper_->GetBytesForNextData();
  size_t fragment_size = fragment_bytes.size();
  if (fragment_size == 0) {
    LOG(ERROR) << "Stream fragment has size 0";
    return;
  }

  // Create shared memory to store the fragment.
  // TODO(dstaessens@) Investigate using import mode when allocating memory.
  base::SharedMemory shm;
  bool success = shm.CreateAndMapAnonymous(fragment_size);
  LOG_ASSERT(success) << "Failed to create shared memory";
  memcpy(shm.memory(), fragment_bytes.data(), fragment_size);
  base::SharedMemoryHandle handle = shm.TakeHandle();
  LOG_ASSERT(handle.IsValid()) << "Failed to take shared memory handle";

  int32_t bitstream_buffer_id = GetNextBitstreamBufferId();
  BitstreamBuffer bitstream_buffer(bitstream_buffer_id, handle, fragment_size);

  DVLOGF(4) << "Bitstream buffer id: " << bitstream_buffer_id;
  decoder_->Decode(bitstream_buffer);
}

void VideoDecoderClient::OnPictureBuffersCreatedTask(
    std::vector<PictureBuffer> buffers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Assigning picture buffers requires an active GL context.
  // TODO(dstaessens@) Investigate making the decoder manage the GL context.
  frame_renderer_->AcquireGLContext();
  decoder_->AssignPictureBuffers(buffers);
  frame_renderer_->ReleaseGLContext();
}

void VideoDecoderClient::OnPictureRenderedTask(int32_t picture_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK(decoder_);
  DVLOGF(4) << "Picture buffer ID: " << picture_buffer_id;

  // Notify the decoder the picture buffer can be reused. The decoder will only
  // request a limited set of picture buffers, when it runs out it will wait
  // until picture buffers are flagged for reuse.
  decoder_->ReusePictureBuffer(picture_buffer_id);
}

int32_t VideoDecoderClient::GetNextBitstreamBufferId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  // The bitstream buffer ID should always be positive, negative values are
  // reserved for uninitialized buffers.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x7FFFFFFF;
  return next_bitstream_buffer_id_;
}

}  // namespace test
}  // namespace media
