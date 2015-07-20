// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gpu_video_encode_accelerator.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/numerics/safe_math.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/media/gpu_video_accelerator_util.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"

#if defined(OS_CHROMEOS)
#if defined(USE_V4L2_CODEC)
#include "content/common/gpu/media/v4l2_video_encode_accelerator.h"
#endif
#if defined(ARCH_CPU_X86_FAMILY)
#include "content/common/gpu/media/vaapi_video_encode_accelerator.h"
#endif
#elif defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
#include "content/common/gpu/media/android_video_encode_accelerator.h"
#endif

namespace content {

static bool MakeDecoderContextCurrent(
    const base::WeakPtr<GpuCommandBufferStub> stub) {
  if (!stub) {
    DLOG(ERROR) << "Stub is gone; won't MakeCurrent().";
    return false;
  }

  if (!stub->decoder()->MakeCurrent()) {
    DLOG(ERROR) << "Failed to MakeCurrent()";
    return false;
  }

  return true;
}

GpuVideoEncodeAccelerator::GpuVideoEncodeAccelerator(int32 host_route_id,
                                                     GpuCommandBufferStub* stub)
    : host_route_id_(host_route_id),
      stub_(stub),
      input_format_(media::PIXEL_FORMAT_UNKNOWN),
      output_buffer_size_(0),
      weak_this_factory_(this) {
  stub_->AddDestructionObserver(this);
  make_context_current_ =
      base::Bind(&MakeDecoderContextCurrent, stub_->AsWeakPtr());
}

GpuVideoEncodeAccelerator::~GpuVideoEncodeAccelerator() {
  // This class can only be self-deleted from OnWillDestroyStub(), which means
  // the VEA has already been destroyed in there.
  DCHECK(!encoder_);
}

void GpuVideoEncodeAccelerator::Initialize(
    media::VideoPixelFormat input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32 initial_bitrate,
    IPC::Message* init_done_msg) {
  DVLOG(2) << "GpuVideoEncodeAccelerator::Initialize(): "
              "input_format=" << input_format
           << ", input_visible_size=" << input_visible_size.ToString()
           << ", output_profile=" << output_profile
           << ", initial_bitrate=" << initial_bitrate;
  DCHECK(!encoder_);

  if (!stub_->channel()->AddRoute(host_route_id_, this)) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::Initialize(): "
                   "failed to add route";
    SendCreateEncoderReply(init_done_msg, false);
    return;
  }

  if (input_visible_size.width() > media::limits::kMaxDimension ||
      input_visible_size.height() > media::limits::kMaxDimension ||
      input_visible_size.GetArea() > media::limits::kMaxCanvas) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::Initialize(): "
                   "input_visible_size " << input_visible_size.ToString()
                << " too large";
    SendCreateEncoderReply(init_done_msg, false);
    return;
  }

  std::vector<GpuVideoEncodeAccelerator::CreateVEAFp>
      create_vea_fps = CreateVEAFps();
  // Try all possible encoders and use the first successful encoder.
  for (size_t i = 0; i < create_vea_fps.size(); ++i) {
    encoder_ = (*create_vea_fps[i])();
    if (encoder_ && encoder_->Initialize(input_format,
                                         input_visible_size,
                                         output_profile,
                                         initial_bitrate,
                                         this)) {
      input_format_ = input_format;
      input_visible_size_ = input_visible_size;
      SendCreateEncoderReply(init_done_msg, true);
      return;
    }
  }
  encoder_.reset();
  DLOG(ERROR)
      << "GpuVideoEncodeAccelerator::Initialize(): VEA initialization failed";
  SendCreateEncoderReply(init_done_msg, false);
}

bool GpuVideoEncodeAccelerator::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoEncodeAccelerator, message)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_Encode, OnEncode)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_UseOutputBitstreamBuffer,
                        OnUseOutputBitstreamBuffer)
    IPC_MESSAGE_HANDLER(
        AcceleratedVideoEncoderMsg_RequestEncodingParametersChange,
        OnRequestEncodingParametersChange)
    IPC_MESSAGE_HANDLER(AcceleratedVideoEncoderMsg_Destroy, OnDestroy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuVideoEncodeAccelerator::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  Send(new AcceleratedVideoEncoderHostMsg_RequireBitstreamBuffers(
      host_route_id_, input_count, input_coded_size, output_buffer_size));
  input_coded_size_ = input_coded_size;
  output_buffer_size_ = output_buffer_size;
}

void GpuVideoEncodeAccelerator::BitstreamBufferReady(int32 bitstream_buffer_id,
                                                     size_t payload_size,
                                                     bool key_frame) {
  Send(new AcceleratedVideoEncoderHostMsg_BitstreamBufferReady(
      host_route_id_, bitstream_buffer_id, payload_size, key_frame));
}

void GpuVideoEncodeAccelerator::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  Send(new AcceleratedVideoEncoderHostMsg_NotifyError(host_route_id_, error));
}

void GpuVideoEncodeAccelerator::OnWillDestroyStub() {
  DCHECK(stub_);
  stub_->channel()->RemoveRoute(host_route_id_);
  stub_->RemoveDestructionObserver(this);
  encoder_.reset();
  delete this;
}

// static
gpu::VideoEncodeAcceleratorSupportedProfiles
GpuVideoEncodeAccelerator::GetSupportedProfiles() {
  media::VideoEncodeAccelerator::SupportedProfiles profiles;
  std::vector<GpuVideoEncodeAccelerator::CreateVEAFp>
      create_vea_fps = CreateVEAFps();

  for (size_t i = 0; i < create_vea_fps.size(); ++i) {
    scoped_ptr<media::VideoEncodeAccelerator>
        encoder = (*create_vea_fps[i])();
    if (!encoder)
      continue;
    media::VideoEncodeAccelerator::SupportedProfiles vea_profiles =
        encoder->GetSupportedProfiles();
    GpuVideoAcceleratorUtil::InsertUniqueEncodeProfiles(
        vea_profiles, &profiles);
  }
  return GpuVideoAcceleratorUtil::ConvertMediaToGpuEncodeProfiles(profiles);
}

// static
std::vector<GpuVideoEncodeAccelerator::CreateVEAFp>
GpuVideoEncodeAccelerator::CreateVEAFps() {
  std::vector<GpuVideoEncodeAccelerator::CreateVEAFp> create_vea_fps;
  create_vea_fps.push_back(&GpuVideoEncodeAccelerator::CreateV4L2VEA);
  create_vea_fps.push_back(&GpuVideoEncodeAccelerator::CreateVaapiVEA);
  create_vea_fps.push_back(&GpuVideoEncodeAccelerator::CreateAndroidVEA);
  return create_vea_fps;
}

// static
scoped_ptr<media::VideoEncodeAccelerator>
GpuVideoEncodeAccelerator::CreateV4L2VEA() {
  scoped_ptr<media::VideoEncodeAccelerator> encoder;
#if defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC)
  scoped_refptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kEncoder);
  if (device)
    encoder.reset(new V4L2VideoEncodeAccelerator(device));
#endif
  return encoder.Pass();
}

// static
scoped_ptr<media::VideoEncodeAccelerator>
GpuVideoEncodeAccelerator::CreateVaapiVEA() {
  scoped_ptr<media::VideoEncodeAccelerator> encoder;
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kDisableVaapiAcceleratedVideoEncode))
    encoder.reset(new VaapiVideoEncodeAccelerator());
#endif
  return encoder.Pass();
}

// static
scoped_ptr<media::VideoEncodeAccelerator>
GpuVideoEncodeAccelerator::CreateAndroidVEA() {
  scoped_ptr<media::VideoEncodeAccelerator> encoder;
#if defined(OS_ANDROID) && defined(ENABLE_WEBRTC)
  encoder.reset(new AndroidVideoEncodeAccelerator());
#endif
  return encoder.Pass();
}

void GpuVideoEncodeAccelerator::OnEncode(
    const AcceleratedVideoEncoderMsg_Encode_Params& params) {
  DVLOG(3) << "GpuVideoEncodeAccelerator::OnEncode: frame_id = "
           << params.frame_id << ", buffer_size=" << params.buffer_size
           << ", force_keyframe=" << params.force_keyframe;
  if (!encoder_)
    return;
  if (params.frame_id < 0) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): invalid "
                   "frame_id=" << params.frame_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  const uint32 aligned_offset =
      params.buffer_offset % base::SysInfo::VMAllocationGranularity();
  base::CheckedNumeric<off_t> map_offset = params.buffer_offset;
  map_offset -= aligned_offset;
  base::CheckedNumeric<size_t> map_size = params.buffer_size;
  map_size += aligned_offset;

  if (!map_offset.IsValid() || !map_size.IsValid()) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode():"
                << " invalid (buffer_offset,buffer_size)";
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(params.buffer_handle, true));
  if (!shm->MapAt(map_offset.ValueOrDie(), map_size.ValueOrDie())) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): "
                   "could not map frame_id=" << params.frame_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  uint8* shm_memory = reinterpret_cast<uint8*>(shm->memory()) + aligned_offset;
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalSharedMemory(
          input_format_,
          input_coded_size_,
          gfx::Rect(input_visible_size_),
          input_visible_size_,
          shm_memory,
          params.buffer_size,
          params.buffer_handle,
          params.buffer_offset,
          base::TimeDelta());
  frame->AddDestructionObserver(
      media::BindToCurrentLoop(
          base::Bind(&GpuVideoEncodeAccelerator::EncodeFrameFinished,
                     weak_this_factory_.GetWeakPtr(),
                     params.frame_id,
                     base::Passed(&shm))));

  if (!frame.get()) {
    DLOG(ERROR) << "GpuVideoEncodeAccelerator::OnEncode(): could not create "
                   "VideoFrame for frame_id=" << params.frame_id;
    NotifyError(media::VideoEncodeAccelerator::kPlatformFailureError);
    return;
  }

  encoder_->Encode(frame, params.force_keyframe);
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

void GpuVideoEncodeAccelerator::OnDestroy() {
  DVLOG(2) << "GpuVideoEncodeAccelerator::OnDestroy()";
  OnWillDestroyStub();
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
  Send(new AcceleratedVideoEncoderHostMsg_NotifyInputDone(host_route_id_,
                                                          frame_id));
  // Just let |shm| fall out of scope.
}

void GpuVideoEncodeAccelerator::Send(IPC::Message* message) {
  stub_->channel()->Send(message);
}

void GpuVideoEncodeAccelerator::SendCreateEncoderReply(IPC::Message* message,
                                                       bool succeeded) {
  GpuCommandBufferMsg_CreateVideoEncoder::WriteReplyParams(message, succeeded);
  Send(message);
}

}  // namespace content
