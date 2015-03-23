// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/video_encoder_shim.h"

#include <inttypes.h>

#include <deque>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "content/renderer/pepper/pepper_video_encoder_host.h"
#include "content/renderer/render_thread_impl.h"
#include "media/cast/cast_config.h"
#include "media/cast/sender/vp8_encoder.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// TODO(llandwerlin): Libvpx doesn't seem to have a maximum frame size
// limitation. We currently limit the size of the frames to encode at
// 1080p (%64 pixels blocks), this seems like a reasonable limit for
// software encoding.
const int32_t kMaxWidth = 1920;
const int32_t kMaxHeight = 1088;

// Bitstream buffer size.
const uint32_t kBitstreamBufferSize = 2 * 1024 * 1024;

// Number of frames needs at any given time.
const uint32_t kInputFrameCount = 1;

class VideoEncoderShim::EncoderImpl {
 public:
  explicit EncoderImpl(const base::WeakPtr<VideoEncoderShim>& shim);
  ~EncoderImpl();

  void Initialize(media::VideoFrame::Format input_format,
                  const gfx::Size& input_visible_size,
                  media::VideoCodecProfile output_profile,
                  uint32 initial_bitrate);
  void Encode(const scoped_refptr<media::VideoFrame>& frame,
              bool force_keyframe);
  void UseOutputBitstreamBuffer(const media::BitstreamBuffer& buffer,
                                uint8_t* mem);
  void RequestEncodingParametersChange(uint32 bitrate, uint32 framerate);
  void Stop();

 private:
  struct PendingEncode {
    PendingEncode(const scoped_refptr<media::VideoFrame>& frame,
                  bool force_keyframe)
        : frame(frame), force_keyframe(force_keyframe) {}
    ~PendingEncode() {}

    scoped_refptr<media::VideoFrame> frame;
    bool force_keyframe;
  };

  struct BitstreamBuffer {
    BitstreamBuffer(const media::BitstreamBuffer buffer, uint8_t* mem)
        : buffer(buffer), mem(mem) {}
    ~BitstreamBuffer() {}

    media::BitstreamBuffer buffer;
    uint8_t* mem;
  };

  void DoEncode();

  base::WeakPtr<VideoEncoderShim> shim_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  scoped_ptr<media::cast::SoftwareVideoEncoder> encoder_;
  std::deque<PendingEncode> frames_;
  std::deque<BitstreamBuffer> buffers_;
};

VideoEncoderShim::EncoderImpl::EncoderImpl(
    const base::WeakPtr<VideoEncoderShim>& shim)
    : shim_(shim), media_task_runner_(base::MessageLoopProxy::current()) {
}

VideoEncoderShim::EncoderImpl::~EncoderImpl() {
}

void VideoEncoderShim::EncoderImpl::Initialize(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32 initial_bitrate) {
  media::cast::VideoSenderConfig config;

  config.max_number_of_video_buffers_used = kInputFrameCount;
  config.number_of_encode_threads = 1;
  encoder_.reset(new media::cast::Vp8Encoder(config));

  encoder_->UpdateRates(initial_bitrate);

  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoEncoderShim::OnRequireBitstreamBuffers, shim_,
                            kInputFrameCount, input_visible_size,
                            kBitstreamBufferSize));
}

void VideoEncoderShim::EncoderImpl::Encode(
    const scoped_refptr<media::VideoFrame>& frame,
    bool force_keyframe) {
  frames_.push_back(PendingEncode(frame, force_keyframe));
  DoEncode();
}

void VideoEncoderShim::EncoderImpl::UseOutputBitstreamBuffer(
    const media::BitstreamBuffer& buffer,
    uint8_t* mem) {
  buffers_.push_back(BitstreamBuffer(buffer, mem));
  DoEncode();
}

void VideoEncoderShim::EncoderImpl::RequestEncodingParametersChange(
    uint32 bitrate,
    uint32 framerate) {
  encoder_->UpdateRates(bitrate);
}

void VideoEncoderShim::EncoderImpl::Stop() {
  frames_.clear();
  buffers_.clear();
  encoder_.reset();
}

void VideoEncoderShim::EncoderImpl::DoEncode() {
  while (!frames_.empty() && !buffers_.empty()) {
    PendingEncode frame = frames_.front();
    frames_.pop_front();

    if (frame.force_keyframe)
      encoder_->GenerateKeyFrame();

    scoped_ptr<media::cast::EncodedFrame> encoded_frame(
        new media::cast::EncodedFrame());
    encoder_->Encode(frame.frame, base::TimeTicks::Now(), encoded_frame.get());

    BitstreamBuffer buffer = buffers_.front();
    buffers_.pop_front();

    CHECK(buffer.buffer.size() >= encoded_frame->data.size());
    memcpy(buffer.mem, encoded_frame->bytes(), encoded_frame->data.size());

    // Pass the media::VideoFrame back to the renderer thread so it's
    // freed on the right thread.
    media_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &VideoEncoderShim::OnBitstreamBufferReady, shim_,
            frame.frame, buffer.buffer.id(), encoded_frame->data.size(),
            encoded_frame->dependency == media::cast::EncodedFrame::KEY));
  }
}

VideoEncoderShim::VideoEncoderShim(PepperVideoEncoderHost* host)
    : host_(host),
      media_task_runner_(
          RenderThreadImpl::current()->GetMediaThreadTaskRunner()),
      weak_ptr_factory_(this) {
  encoder_impl_.reset(new EncoderImpl(weak_ptr_factory_.GetWeakPtr()));
}

VideoEncoderShim::~VideoEncoderShim() {
  DCHECK(RenderThreadImpl::current());

  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoEncoderShim::EncoderImpl::Stop,
                            base::Owned(encoder_impl_.release())));
}

std::vector<media::VideoEncodeAccelerator::SupportedProfile>
VideoEncoderShim::GetSupportedProfiles() {
  media::VideoEncodeAccelerator::SupportedProfile profile = {
      media::VP8PROFILE_ANY,
      gfx::Size(kMaxWidth, kMaxHeight),
      media::cast::kDefaultMaxFrameRate,
      1};
  std::vector<media::VideoEncodeAccelerator::SupportedProfile> profiles;
  profiles.push_back(profile);
  return profiles;
}

bool VideoEncoderShim::Initialize(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32 initial_bitrate,
    media::VideoEncodeAccelerator::Client* client) {
  DCHECK(RenderThreadImpl::current());
  DCHECK_EQ(client, host_);

  if (input_format != media::VideoFrame::I420)
    return false;

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoEncoderShim::EncoderImpl::Initialize,
                 base::Unretained(encoder_impl_.get()), input_format,
                 input_visible_size, output_profile, initial_bitrate));

  return true;
}

void VideoEncoderShim::Encode(const scoped_refptr<media::VideoFrame>& frame,
                              bool force_keyframe) {
  DCHECK(RenderThreadImpl::current());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoEncoderShim::EncoderImpl::Encode,
                 base::Unretained(encoder_impl_.get()), frame, force_keyframe));
}

void VideoEncoderShim::UseOutputBitstreamBuffer(
    const media::BitstreamBuffer& buffer) {
  DCHECK(RenderThreadImpl::current());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoEncoderShim::EncoderImpl::UseOutputBitstreamBuffer,
                 base::Unretained(encoder_impl_.get()), buffer,
                 host_->ShmHandleToAddress(buffer.id())));
}

void VideoEncoderShim::RequestEncodingParametersChange(uint32 bitrate,
                                                       uint32 framerate) {
  DCHECK(RenderThreadImpl::current());

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &VideoEncoderShim::EncoderImpl::RequestEncodingParametersChange,
          base::Unretained(encoder_impl_.get()), bitrate, framerate));
}

void VideoEncoderShim::Destroy() {
  DCHECK(RenderThreadImpl::current());

  delete this;
}

void VideoEncoderShim::OnRequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK(RenderThreadImpl::current());

  host_->RequireBitstreamBuffers(input_count, input_coded_size,
                                 output_buffer_size);
}

void VideoEncoderShim::OnBitstreamBufferReady(
    scoped_refptr<media::VideoFrame> frame,
    int32 bitstream_buffer_id,
    size_t payload_size,
    bool key_frame) {
  DCHECK(RenderThreadImpl::current());

  host_->BitstreamBufferReady(bitstream_buffer_id, payload_size, key_frame);
}

void VideoEncoderShim::OnNotifyError(
    media::VideoEncodeAccelerator::Error error) {
  DCHECK(RenderThreadImpl::current());

  host_->NotifyError(error);
}

}  // namespace content
