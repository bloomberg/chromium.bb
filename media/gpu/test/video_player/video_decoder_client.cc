// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_decoder_client.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_player/frame_renderer.h"

#define DVLOGF(level) DVLOG(level) << __func__ << "(): "
#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace media {
namespace test {

VideoDecoderClient::VideoDecoderClient(
    const VideoPlayer::EventCallback& event_cb,
    FrameRenderer* renderer,
    VideoFrameValidator* frame_validator)
    : event_cb_(event_cb),
      frame_renderer_(renderer),
      frame_validator_(frame_validator),
      decoder_client_thread_("VDAClientDecoderThread"),
      decoder_client_state_(VideoDecoderClientState::kUninitialized),
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
    FrameRenderer* frame_renderer,
    VideoFrameValidator* frame_validator) {
  auto decoder_client = base::WrapUnique(
      new VideoDecoderClient(event_cb, frame_renderer, frame_validator));
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

void VideoDecoderClient::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::PlayTask, weak_this_));
}

void VideoDecoderClient::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::FlushTask, weak_this_));
}

void VideoDecoderClient::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::ResetTask, weak_this_));
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

  // Create a set of picture buffers and give them to the decoder.
  std::vector<PictureBuffer> picture_buffers;
  for (uint32_t i = 0; i < requested_num_of_buffers; ++i)
    picture_buffers.emplace_back(GetNextPictureBufferId(), size);
  decoder_->AssignPictureBuffers(picture_buffers);

  // Create a video frame for each of the picture buffers and provide memory
  // handles to the video frame's data to the decoder.
  for (const PictureBuffer& picture_buffer : picture_buffers) {
    scoped_refptr<VideoFrame> video_frame =
        CreateVideoFrame(pixel_format, size);
    LOG_ASSERT(video_frame) << "Failed to create video frame";
    video_frames_.emplace(picture_buffer.id(), video_frame);
    gfx::GpuMemoryBufferHandle handle =
        CreateGpuMemoryBufferHandle(video_frame);
    LOG_ASSERT(!handle.is_null()) << "Failed to create GPU memory handle";
    decoder_->ImportBufferForPicture(picture_buffer.id(), pixel_format, handle);
  }
}

void VideoDecoderClient::DismissPictureBuffer(int32_t picture_buffer_id) {
  // TODO(dstaessens@) support dismissing picture buffers.
  NOTIMPLEMENTED();
}

void VideoDecoderClient::PictureReady(const Picture& picture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4) << "Picture buffer ID: " << picture.picture_buffer_id();

  event_cb_.Run(VideoPlayerEvent::kFrameDecoded);

  auto it = video_frames_.find(picture.picture_buffer_id());
  LOG_ASSERT(it != video_frames_.end());
  scoped_refptr<VideoFrame> video_frame = it->second;

  // Wrap the video frame in another video frame that calls
  // ReusePictureBufferTask() upon destruction. When the renderer is done using
  // the video frame, the associated picture buffer will automatically be
  // flagged for reuse.
  base::OnceClosure delete_cb = BindToCurrentLoop(
      base::BindOnce(&VideoDecoderClient::ReusePictureBufferTask,
                     base::Unretained(this), picture.picture_buffer_id()));

  scoped_refptr<VideoFrame> wrapped_video_frame = VideoFrame::WrapVideoFrame(
      video_frame, video_frame->format(), video_frame->visible_rect(),
      video_frame->visible_rect().size());
  wrapped_video_frame->AddDestructionObserver(std::move(delete_cb));

  frame_renderer_->RenderFrame(wrapped_video_frame);

  frame_validator_->EvaluateVideoFrame(video_frame, current_frame_index_++);
}

void VideoDecoderClient::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Queue the next fragment to be decoded. Flush when we reached the end of the
  // stream.
  if (encoded_data_helper_->ReachEndOfStream()) {
    decoder_client_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VideoDecoderClient::FlushTask, weak_this_));
  } else {
    decoder_client_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VideoDecoderClient::DecodeNextFragmentTask,
                                  weak_this_));
  }
}

void VideoDecoderClient::NotifyFlushDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);

  decoder_client_state_ = VideoDecoderClientState::kIdle;
  event_cb_.Run(VideoPlayerEvent::kFlushDone);
}

void VideoDecoderClient::NotifyResetDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);

  // We finished resetting to a different point in the stream, so we should
  // update the frame index. Currently only resetting to the start of the stream
  // is supported, so we can set the frame index to zero here.
  current_frame_index_ = 0;

  decoder_client_state_ = VideoDecoderClientState::kIdle;
  event_cb_.Run(VideoPlayerEvent::kResetDone);
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

  frame_renderer_->AcquireGLContext();
  bool hasGLContext = frame_renderer_->GetGLContext() != nullptr;
  frame_renderer_->ReleaseGLContext();

  if (hasGLContext) {
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

  decoder_client_state_ = VideoDecoderClientState::kIdle;
  done->Signal();
}

void VideoDecoderClient::DestroyDecoderTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Invalidate all scheduled tasks.
  weak_this_factory_.InvalidateWeakPtrs();

  // Destroying a decoder requires an active GL context.
  // TODO(dstaessens@) Investigate making the decoder manage the GL context.
  if (decoder_) {
    frame_renderer_->AcquireGLContext();
    decoder_.reset();
    frame_renderer_->ReleaseGLContext();
  }

  decoder_client_state_ = VideoDecoderClientState::kUninitialized;
  done->Signal();
}

void VideoDecoderClient::DecodeNextFragmentTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Stop decoding fragments if we're no longer in the decoding state.
  if (decoder_client_state_ != VideoDecoderClientState::kDecoding)
    return;

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

void VideoDecoderClient::PlayTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // This method should only be called when the decoder client is idle. If
  // called e.g. while flushing, the behavior is undefined.
  ASSERT_EQ(decoder_client_state_, VideoDecoderClientState::kIdle);

  // Start decoding the first fragment. While in the decoding state new
  // fragments will automatically be fed to the decoder, when the decoder
  // notifies us it reached the end of the current bitstream.
  decoder_client_state_ = VideoDecoderClientState::kDecoding;
  DecodeNextFragmentTask();
}

void VideoDecoderClient::FlushTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Changing the state to flushing will abort any pending decodes.
  decoder_client_state_ = VideoDecoderClientState::kFlushing;
  decoder_->Flush();
  event_cb_.Run(VideoPlayerEvent::kFlushing);
}

void VideoDecoderClient::ResetTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Changing the state to resetting will abort any pending decodes.
  decoder_client_state_ = VideoDecoderClientState::kResetting;
  // TODO(dstaessens@) Allow resetting to any point in the stream.
  encoded_data_helper_->Rewind();
  decoder_->Reset();
  event_cb_.Run(VideoPlayerEvent::kResetting);
}

void VideoDecoderClient::ReusePictureBufferTask(int32_t picture_buffer_id) {
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

int32_t VideoDecoderClient::GetNextPictureBufferId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  // The picture buffer ID should always be positive, negative values are
  // reserved for uninitialized buffers.
  next_picture_buffer_id_ = (next_picture_buffer_id_ + 1) & 0x7FFFFFFF;
  return next_picture_buffer_id_;
}

}  // namespace test
}  // namespace media
