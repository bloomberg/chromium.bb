// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gpu_video_encode_accelerator.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop_proxy.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL) && defined(USE_X11)
#include "content/common/gpu/media/exynos_video_encode_accelerator.h"
#elif defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
#include "content/common/gpu/media/android_video_encode_accelerator.h"
#endif

namespace content {

GpuVideoEncodeAccelerator::GpuVideoEncodeAccelerator(GpuChannel* gpu_channel,
                                                     int32 route_id)
    : weak_this_factory_(this),
      channel_(gpu_channel),
      route_id_(route_id),
      input_format_(media::VideoFrame::UNKNOWN),
      output_buffer_size_(0) {}

GpuVideoEncodeAccelerator::~GpuVideoEncodeAccelerator() {
  if (encoder_)
    encoder_.release()->Destroy();
}

bool GpuVideoEncodeAccelerator::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoEncodeAccelerator, message)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_Initialize, OnInitialize)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_Encode, OnEncode)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_UseOutputBitstreamBuffer,
                        OnUseOutputBitstreamBuffer)
    IPC_MESSAGE_HANDLER(
        AcceleratedVideoEncoderMsg_RequestEncodingParametersChange,
        OnRequestEncodingParametersChange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuVideoEncodeAccelerator::OnChannelError() {
  NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
  if (channel_)
    channel_ = NULL;
}

void GpuVideoEncodeAccelerator::NotifyInitializeDone() {
  Send(new AcceleratedVideoEncoderHostMsg_NotifyInitializeDone(route_id_));
}

void GpuVideoEncodeAccelerator::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  Send(new AcceleratedVideoEncoderHostMsg_RequireBitstreamBuffers(
      route_id_, input_count, input_coded_size, output_buffer_size));
  input_coded_size_ = input_coded_size;
  output_buffer_size_ = output_buffer_size;
}

void GpuVideoEncodeAccelerator::BitstreamBufferReady(int32 bitstream_buffer_id,
                                                     size_t payload_size,
                                                     bool key_frame) {
  Send(new AcceleratedVideoEncoderHostMsg_BitstreamBufferReady(
      route_id_, bitstream_buffer_id, payload_size, key_frame));
}

void GpuVideoEncodeAccelerator::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  Send(new AcceleratedVideoEncoderHostMsg_NotifyError(route_id_, error));
}

// static
std::vector<media::VideoEncodeAccelerator::SupportedProfile>
GpuVideoEncodeAccelerator::GetSupportedProfiles() {
  std::vector<media::VideoEncodeAccelerator::SupportedProfile> profiles;

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL) && defined(USE_X11)
  profiles = ExynosVideoEncodeAccelerator::GetSupportedProfiles();
#elif defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  profiles = AndroidVideoEncodeAccelerator::GetSupportedProfiles();
#endif

  // TODO(sheu): return platform-specific profiles.
  return profiles;
}

void GpuVideoEncodeAccelerator::CreateEncoder() {
  DCHECK(!encoder_);
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL) && defined(USE_X11)
  encoder_.reset(new ExynosVideoEncodeAccelerator(this));
#elif defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  encoder_.reset(new AndroidVideoEncodeAccelerator(this));
#endif
}

void GpuVideoEncodeAccelerator::OnInitialize(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32 initial_bitrate) {
  DVLOG(2) << "GpuVideoEncodeAccelerator::OnInitialize(): "
              "input_format=" << input_format
           << ", input_visible_size=" << input_visible_size.ToString()
           << ", output_profile=" << output_profile
           << ", initial_bitrate=" << initial_bitrate;
  DCHECK(!encoder_);

  if (input_visible_size.width() > media::limits::kMaxDimension ||
      input_visible_size.height() > media::limits::kMaxDimension ||
      input_visible_size.GetArea() > media::limits::kMaxCanvas) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnInitialize(): "
                   "input_visible_size " << input_visible_size.ToString()
                << " too large";
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  CreateEncoder();
  if (!encoder_) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnInitialize(): VEA creation "
                   "failed";
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  encoder_->Initialize(
      input_format, input_visible_size, output_profile, initial_bitrate);
  input_format_ = input_format;
  input_visible_size_ = input_visible_size;
}

void GpuVideoEncodeAccelerator::OnEncode(int32 frame_id,
                                         base::SharedMemoryHandle buffer_handle,
                                         uint32 buffer_size,
                                         bool force_keyframe) {
  DVLOG(3) << "GpuVideoEncodeAccelerator::OnEncode(): frame_id=" << frame_id
           << ", buffer_size=" << buffer_size
           << ", force_keyframe=" << force_keyframe;
  if (!encoder_)
    return;
  if (frame_id < 0) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): invalid frame_id="
                << frame_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(buffer_handle, true));
  if (!shm->Map(buffer_size)) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): "
                   "could not map frame_id=" << frame_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  uint8* shm_memory = reinterpret_cast<uint8*>(shm->memory());
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalPackedMemory(
          input_format_,
          input_coded_size_,
          gfx::Rect(input_visible_size_),
          input_visible_size_,
          shm_memory,
          buffer_size,
          buffer_handle,
          base::TimeDelta(),
          // It's turtles all the way down...
          base::Bind(base::IgnoreResult(&base::MessageLoopProxy::PostTask),
                     base::MessageLoopProxy::current(),
                     FROM_HERE,
                     base::Bind(&GpuVideoEncodeAccelerator::EncodeFrameFinished,
                                weak_this_factory_.GetWeakPtr(),
                                frame_id,
                                base::Passed(&shm))));

  if (!frame) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): "
                   "could not create VideoFrame for frame_id=" << frame_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  encoder_->Encode(frame, force_keyframe);
}

void GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(
    int32 buffer_id,
    base::SharedMemoryHandle buffer_handle,
    uint32 buffer_size) {
  DVLOG(3) << "GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(): "
              "buffer_id=" << buffer_id
           << ", buffer_size=" << buffer_size;
  if (!encoder_)
    return;
  if (buffer_id < 0) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(): "
                   "invalid buffer_id=" << buffer_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  if (buffer_size < output_buffer_size_) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnUseOutputBitstreamBuffer(): "
                   "buffer too small for buffer_id=" << buffer_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
  encoder_->UseOutputBitstreamBuffer(
      media::BitstreamBuffer(buffer_id, buffer_handle, buffer_size));
}

void GpuVideoEncodeAccelerator::OnRequestEncodingParametersChange(
    uint32 bitrate,
    uint32 framerate) {
  DVLOG(2) << "GpuVideoEncodeAccelerator::OnRequestEncodingParametersChange(): "
              "bitrate=" << bitrate
           << ", framerate=" << framerate;
  if (!encoder_)
    return;
  encoder_->RequestEncodingParametersChange(bitrate, framerate);
}

void GpuVideoEncodeAccelerator::EncodeFrameFinished(
    int32 frame_id,
    scoped_ptr<base::SharedMemory> shm) {
  Send(new AcceleratedVideoEncoderHostMsg_NotifyInputDone(route_id_, frame_id));
  // Just let shm fall out of scope.
}

void GpuVideoEncodeAccelerator::Send(IPC::Message* message) {
  if (!channel_) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::Send(): no channel";
    delete message;
    return;
  } else if (!channel_->Send(message)) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::Send(): sending failed: "
                   "message->type()=" << message->type();
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }
}

}  // namespace content
