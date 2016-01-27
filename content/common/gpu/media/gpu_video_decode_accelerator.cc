// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gpu_video_decode_accelerator.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"

#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/media/gpu_video_accelerator_util.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/message_filter.h"
#include "media/base/limits.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "content/common/gpu/media/dxva_video_decode_accelerator_win.h"
#elif defined(OS_MACOSX)
#include "content/common/gpu/media/vt_video_decode_accelerator_mac.h"
#elif defined(OS_CHROMEOS)
#if defined(USE_V4L2_CODEC)
#include "content/common/gpu/media/v4l2_device.h"
#include "content/common/gpu/media/v4l2_slice_video_decode_accelerator.h"
#include "content/common/gpu/media/v4l2_video_decode_accelerator.h"
#endif
#if defined(ARCH_CPU_X86_FAMILY)
#include "content/common/gpu/media/vaapi_video_decode_accelerator.h"
#include "ui/gl/gl_implementation.h"
#endif
#elif defined(USE_OZONE)
#include "media/ozone/media_ozone_platform.h"
#elif defined(OS_ANDROID)
#include "content/common/gpu/media/android_video_decode_accelerator.h"
#endif

#include "ui/gfx/geometry/size.h"

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

// DebugAutoLock works like AutoLock but only acquires the lock when
// DCHECK is on.
#if DCHECK_IS_ON()
typedef base::AutoLock DebugAutoLock;
#else
class DebugAutoLock {
 public:
  explicit DebugAutoLock(base::Lock&) {}
};
#endif

class GpuVideoDecodeAccelerator::MessageFilter : public IPC::MessageFilter {
 public:
  MessageFilter(GpuVideoDecodeAccelerator* owner, int32_t host_route_id)
      : owner_(owner), host_route_id_(host_route_id) {}

  void OnChannelError() override { sender_ = NULL; }

  void OnChannelClosing() override { sender_ = NULL; }

  void OnFilterAdded(IPC::Sender* sender) override { sender_ = sender; }

  void OnFilterRemoved() override {
    // This will delete |owner_| and |this|.
    owner_->OnFilterRemoved();
  }

  bool OnMessageReceived(const IPC::Message& msg) override {
    if (msg.routing_id() != host_route_id_)
      return false;

    IPC_BEGIN_MESSAGE_MAP(MessageFilter, msg)
      IPC_MESSAGE_FORWARD(AcceleratedVideoDecoderMsg_Decode, owner_,
                          GpuVideoDecodeAccelerator::OnDecode)
      IPC_MESSAGE_UNHANDLED(return false)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  bool SendOnIOThread(IPC::Message* message) {
    DCHECK(!message->is_sync());
    if (!sender_) {
      delete message;
      return false;
    }
    return sender_->Send(message);
  }

 protected:
  ~MessageFilter() override {}

 private:
  GpuVideoDecodeAccelerator* const owner_;
  const int32_t host_route_id_;
  // The sender to which this filter was added.
  IPC::Sender* sender_;
};

GpuVideoDecodeAccelerator::GpuVideoDecodeAccelerator(
    int32_t host_route_id,
    GpuCommandBufferStub* stub,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : host_route_id_(host_route_id),
      stub_(stub),
      texture_target_(0),
      filter_removed_(true, false),
      child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      weak_factory_for_io_(this) {
  DCHECK(stub_);
  stub_->AddDestructionObserver(this);
  make_context_current_ =
      base::Bind(&MakeDecoderContextCurrent, stub_->AsWeakPtr());
}

GpuVideoDecodeAccelerator::~GpuVideoDecodeAccelerator() {
  // This class can only be self-deleted from OnWillDestroyStub(), which means
  // the VDA has already been destroyed in there.
  DCHECK(!video_decode_accelerator_);
}

// static
gpu::VideoDecodeAcceleratorCapabilities
GpuVideoDecodeAccelerator::GetCapabilities() {
  media::VideoDecodeAccelerator::Capabilities capabilities;
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableAcceleratedVideoDecode))
    return gpu::VideoDecodeAcceleratorCapabilities();

  // Query supported profiles for each VDA. The order of querying VDAs should
  // be the same as the order of initializing VDAs. Then the returned profile
  // can be initialized by corresponding VDA successfully.
#if defined(OS_WIN)
  capabilities.supported_profiles =
      DXVAVideoDecodeAccelerator::GetSupportedProfiles();
#elif defined(OS_CHROMEOS)
  media::VideoDecodeAccelerator::SupportedProfiles vda_profiles;
#if defined(USE_V4L2_CODEC)
  vda_profiles = V4L2VideoDecodeAccelerator::GetSupportedProfiles();
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      vda_profiles, &capabilities.supported_profiles);
  vda_profiles = V4L2SliceVideoDecodeAccelerator::GetSupportedProfiles();
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      vda_profiles, &capabilities.supported_profiles);
#endif
#if defined(ARCH_CPU_X86_FAMILY)
  vda_profiles = VaapiVideoDecodeAccelerator::GetSupportedProfiles();
  GpuVideoAcceleratorUtil::InsertUniqueDecodeProfiles(
      vda_profiles, &capabilities.supported_profiles);
#endif
#elif defined(OS_MACOSX)
  capabilities.supported_profiles =
      VTVideoDecodeAccelerator::GetSupportedProfiles();
#elif defined(OS_ANDROID)
  capabilities = AndroidVideoDecodeAccelerator::GetCapabilities();
#endif
  return GpuVideoAcceleratorUtil::ConvertMediaToGpuDecodeCapabilities(
      capabilities);
}

bool GpuVideoDecodeAccelerator::OnMessageReceived(const IPC::Message& msg) {
  if (!video_decode_accelerator_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecodeAccelerator, msg)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_SetCdm, OnSetCdm)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Decode, OnDecode)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_AssignPictureBuffers,
                        OnAssignPictureBuffers)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_ReusePictureBuffer,
                        OnReusePictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Flush, OnFlush)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Destroy, OnDestroy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuVideoDecodeAccelerator::NotifyCdmAttached(bool success) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_CdmAttached(host_route_id_,
                                                           success)))
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_CdmAttached) failed";
}

void GpuVideoDecodeAccelerator::ProvidePictureBuffers(
    uint32_t requested_num_of_buffers,
    const gfx::Size& dimensions,
    uint32_t texture_target) {
  if (dimensions.width() > media::limits::kMaxDimension ||
      dimensions.height() > media::limits::kMaxDimension ||
      dimensions.GetArea() > media::limits::kMaxCanvas) {
    NotifyError(media::VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  if (!Send(new AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers(
           host_route_id_,
           requested_num_of_buffers,
           dimensions,
           texture_target))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers) "
                << "failed";
  }
  texture_dimensions_ = dimensions;
  texture_target_ = texture_target;
}

void GpuVideoDecodeAccelerator::DismissPictureBuffer(
    int32_t picture_buffer_id) {
  // Notify client that picture buffer is now unused.
  if (!Send(new AcceleratedVideoDecoderHostMsg_DismissPictureBuffer(
          host_route_id_, picture_buffer_id))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer) "
                << "failed";
  }
  DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
  uncleared_textures_.erase(picture_buffer_id);
}

void GpuVideoDecodeAccelerator::PictureReady(
    const media::Picture& picture) {
  // VDA may call PictureReady on IO thread. SetTextureCleared should run on
  // the child thread. VDA is responsible to call PictureReady on the child
  // thread when a picture buffer is delivered the first time.
  if (child_task_runner_->BelongsToCurrentThread()) {
    SetTextureCleared(picture);
  } else {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
    DCHECK_EQ(0u, uncleared_textures_.count(picture.picture_buffer_id()));
  }

  if (!Send(new AcceleratedVideoDecoderHostMsg_PictureReady(
          host_route_id_, picture.picture_buffer_id(),
          picture.bitstream_buffer_id(), picture.visible_rect(),
          picture.allow_overlay()))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_PictureReady) failed";
  }
}

void GpuVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int32_t bitstream_buffer_id) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed(
          host_route_id_, bitstream_buffer_id))) {
    DLOG(ERROR)
        << "Send(AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed) "
        << "failed";
  }
}

void GpuVideoDecodeAccelerator::NotifyFlushDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_FlushDone(host_route_id_)))
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_FlushDone) failed";
}

void GpuVideoDecodeAccelerator::NotifyResetDone() {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ResetDone(host_route_id_)))
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ResetDone) failed";
}

void GpuVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  if (!Send(new AcceleratedVideoDecoderHostMsg_ErrorNotification(
          host_route_id_, error))) {
    DLOG(ERROR) << "Send(AcceleratedVideoDecoderHostMsg_ErrorNotification) "
                << "failed";
  }
}

void GpuVideoDecodeAccelerator::OnWillDestroyStub() {
  // The stub is going away, so we have to stop and destroy VDA here, before
  // returning, because the VDA may need the GL context to run and/or do its
  // cleanup. We cannot destroy the VDA before the IO thread message filter is
  // removed however, since we cannot service incoming messages with VDA gone.
  // We cannot simply check for existence of VDA on IO thread though, because
  // we don't want to synchronize the IO thread with the ChildThread.
  // So we have to wait for the RemoveFilter callback here instead and remove
  // the VDA after it arrives and before returning.
  if (filter_) {
    stub_->channel()->RemoveFilter(filter_.get());
    filter_removed_.Wait();
  }

  stub_->channel()->RemoveRoute(host_route_id_);
  stub_->RemoveDestructionObserver(this);

  video_decode_accelerator_.reset();
  delete this;
}

bool GpuVideoDecodeAccelerator::Send(IPC::Message* message) {
  if (filter_ && io_task_runner_->BelongsToCurrentThread())
    return filter_->SendOnIOThread(message);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  return stub_->channel()->Send(message);
}

void GpuVideoDecodeAccelerator::Initialize(
    const media::VideoDecodeAccelerator::Config& config,
    IPC::Message* init_done_msg) {
  DCHECK(!video_decode_accelerator_);

  if (!stub_->channel()->AddRoute(host_route_id_, this)) {
    DLOG(ERROR) << "Initialize(): failed to add route";
    SendCreateDecoderReply(init_done_msg, false);
  }

#if !defined(OS_WIN)
  // Ensure we will be able to get a GL context at all before initializing
  // non-Windows VDAs.
  if (!make_context_current_.Run()) {
    SendCreateDecoderReply(init_done_msg, false);
    return;
  }
#endif

  // Array of Create..VDA() function pointers, maybe applicable to the current
  // platform. This list is ordered by priority of use and it should be the
  // same as the order of querying supported profiles of VDAs.
  const GpuVideoDecodeAccelerator::CreateVDAFp create_vda_fps[] = {
      &GpuVideoDecodeAccelerator::CreateDXVAVDA,
      &GpuVideoDecodeAccelerator::CreateV4L2VDA,
      &GpuVideoDecodeAccelerator::CreateV4L2SliceVDA,
      &GpuVideoDecodeAccelerator::CreateVaapiVDA,
      &GpuVideoDecodeAccelerator::CreateVTVDA,
      &GpuVideoDecodeAccelerator::CreateOzoneVDA,
      &GpuVideoDecodeAccelerator::CreateAndroidVDA};

  for (const auto& create_vda_function : create_vda_fps) {
    video_decode_accelerator_ = (this->*create_vda_function)();
    if (!video_decode_accelerator_ ||
        !video_decode_accelerator_->Initialize(config, this))
      continue;

    if (video_decode_accelerator_->CanDecodeOnIOThread()) {
      filter_ = new MessageFilter(this, host_route_id_);
      stub_->channel()->AddFilter(filter_.get());
    }
    SendCreateDecoderReply(init_done_msg, true);
    return;
  }
  video_decode_accelerator_.reset();
  LOG(ERROR) << "HW video decode not available for profile " << config.profile
             << (config.is_encrypted ? " with encryption" : "");
  SendCreateDecoderReply(init_done_msg, false);
}

scoped_ptr<media::VideoDecodeAccelerator>
GpuVideoDecodeAccelerator::CreateDXVAVDA() {
  scoped_ptr<media::VideoDecodeAccelerator> decoder;
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN7) {
    DVLOG(0) << "Initializing DXVA HW decoder for windows.";
    decoder.reset(new DXVAVideoDecodeAccelerator(make_context_current_,
        stub_->decoder()->GetGLContext()));
  } else {
    NOTIMPLEMENTED() << "HW video decode acceleration not available.";
  }
#endif
  return decoder;
}

scoped_ptr<media::VideoDecodeAccelerator>
GpuVideoDecodeAccelerator::CreateV4L2VDA() {
  scoped_ptr<media::VideoDecodeAccelerator> decoder;
#if defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC)
  scoped_refptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kDecoder);
  if (device.get()) {
    decoder.reset(new V4L2VideoDecodeAccelerator(
        gfx::GLSurfaceEGL::GetHardwareDisplay(),
        stub_->decoder()->GetGLContext()->GetHandle(),
        weak_factory_for_io_.GetWeakPtr(),
        make_context_current_,
        device,
        io_task_runner_));
  }
#endif
  return decoder;
}

scoped_ptr<media::VideoDecodeAccelerator>
GpuVideoDecodeAccelerator::CreateV4L2SliceVDA() {
  scoped_ptr<media::VideoDecodeAccelerator> decoder;
#if defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC)
  scoped_refptr<V4L2Device> device = V4L2Device::Create(V4L2Device::kDecoder);
  if (device.get()) {
    decoder.reset(new V4L2SliceVideoDecodeAccelerator(
        device,
        gfx::GLSurfaceEGL::GetHardwareDisplay(),
        stub_->decoder()->GetGLContext()->GetHandle(),
        weak_factory_for_io_.GetWeakPtr(),
        make_context_current_,
        io_task_runner_));
  }
#endif
  return decoder;
}

void GpuVideoDecodeAccelerator::BindImage(uint32_t client_texture_id,
                                          uint32_t texture_target,
                                          scoped_refptr<gl::GLImage> image) {
  gpu::gles2::GLES2Decoder* command_decoder = stub_->decoder();
  gpu::gles2::TextureManager* texture_manager =
      command_decoder->GetContextGroup()->texture_manager();
  gpu::gles2::TextureRef* ref = texture_manager->GetTexture(client_texture_id);
  if (ref) {
    texture_manager->SetLevelImage(ref, texture_target, 0, image.get(),
                                   gpu::gles2::Texture::BOUND);
  }
}

scoped_ptr<media::VideoDecodeAccelerator>
GpuVideoDecodeAccelerator::CreateVaapiVDA() {
  scoped_ptr<media::VideoDecodeAccelerator> decoder;
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  decoder.reset(new VaapiVideoDecodeAccelerator(
      make_context_current_, base::Bind(&GpuVideoDecodeAccelerator::BindImage,
                                        base::Unretained(this))));
#endif
  return decoder;
}

scoped_ptr<media::VideoDecodeAccelerator>
GpuVideoDecodeAccelerator::CreateVTVDA() {
  scoped_ptr<media::VideoDecodeAccelerator> decoder;
#if defined(OS_MACOSX)
  decoder.reset(new VTVideoDecodeAccelerator(
      make_context_current_, base::Bind(&GpuVideoDecodeAccelerator::BindImage,
                                        base::Unretained(this))));
#endif
  return decoder;
}

scoped_ptr<media::VideoDecodeAccelerator>
GpuVideoDecodeAccelerator::CreateOzoneVDA() {
  scoped_ptr<media::VideoDecodeAccelerator> decoder;
#if !defined(OS_CHROMEOS) && defined(USE_OZONE)
  media::MediaOzonePlatform* platform =
      media::MediaOzonePlatform::GetInstance();
  decoder.reset(platform->CreateVideoDecodeAccelerator(make_context_current_));
#endif
  return decoder;
}

scoped_ptr<media::VideoDecodeAccelerator>
GpuVideoDecodeAccelerator::CreateAndroidVDA() {
  scoped_ptr<media::VideoDecodeAccelerator> decoder;
#if defined(OS_ANDROID)
  decoder.reset(new AndroidVideoDecodeAccelerator(stub_->decoder()->AsWeakPtr(),
                                                  make_context_current_));
#endif
  return decoder;
}

void GpuVideoDecodeAccelerator::OnSetCdm(int cdm_id) {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->SetCdm(cdm_id);
}

// Runs on IO thread if video_decode_accelerator_->CanDecodeOnIOThread() is
// true, otherwise on the main thread.
void GpuVideoDecodeAccelerator::OnDecode(
    const AcceleratedVideoDecoderMsg_Decode_Params& params) {
  DCHECK(video_decode_accelerator_);
  if (params.bitstream_buffer_id < 0) {
    DLOG(ERROR) << "BitstreamBuffer id " << params.bitstream_buffer_id
                << " out of range";
    if (child_task_runner_->BelongsToCurrentThread()) {
      NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
    } else {
      child_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&GpuVideoDecodeAccelerator::NotifyError,
                     base::Unretained(this),
                     media::VideoDecodeAccelerator::INVALID_ARGUMENT));
    }
    return;
  }

  media::BitstreamBuffer bitstream_buffer(params.bitstream_buffer_id,
                                          params.buffer_handle, params.size,
                                          params.presentation_timestamp);
  if (!params.key_id.empty()) {
    bitstream_buffer.SetDecryptConfig(
        media::DecryptConfig(params.key_id, params.iv, params.subsamples));
  }

  video_decode_accelerator_->Decode(bitstream_buffer);
}

void GpuVideoDecodeAccelerator::OnAssignPictureBuffers(
    const std::vector<int32_t>& buffer_ids,
    const std::vector<uint32_t>& texture_ids) {
  if (buffer_ids.size() != texture_ids.size()) {
    NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
    return;
  }

  gpu::gles2::GLES2Decoder* command_decoder = stub_->decoder();
  gpu::gles2::TextureManager* texture_manager =
      command_decoder->GetContextGroup()->texture_manager();

  std::vector<media::PictureBuffer> buffers;
  std::vector<scoped_refptr<gpu::gles2::TextureRef> > textures;
  for (uint32_t i = 0; i < buffer_ids.size(); ++i) {
    if (buffer_ids[i] < 0) {
      DLOG(ERROR) << "Buffer id " << buffer_ids[i] << " out of range";
      NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
      return;
    }
    gpu::gles2::TextureRef* texture_ref = texture_manager->GetTexture(
        texture_ids[i]);
    if (!texture_ref) {
      DLOG(ERROR) << "Failed to find texture id " << texture_ids[i];
      NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
      return;
    }
    gpu::gles2::Texture* info = texture_ref->texture();
    if (info->target() != texture_target_) {
      DLOG(ERROR) << "Texture target mismatch for texture id "
                  << texture_ids[i];
      NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
      return;
    }
    if (texture_target_ == GL_TEXTURE_EXTERNAL_OES ||
        texture_target_ == GL_TEXTURE_RECTANGLE_ARB) {
      // These textures have their dimensions defined by the underlying storage.
      // Use |texture_dimensions_| for this size.
      texture_manager->SetLevelInfo(
          texture_ref, texture_target_, 0, GL_RGBA, texture_dimensions_.width(),
          texture_dimensions_.height(), 1, 0, GL_RGBA, 0, gfx::Rect());
    } else {
      // For other targets, texture dimensions should already be defined.
      GLsizei width = 0, height = 0;
      info->GetLevelSize(texture_target_, 0, &width, &height, nullptr);
      if (width != texture_dimensions_.width() ||
          height != texture_dimensions_.height()) {
        DLOG(ERROR) << "Size mismatch for texture id " << texture_ids[i];
        NotifyError(media::VideoDecodeAccelerator::INVALID_ARGUMENT);
        return;
      }

      // TODO(dshwang): after moving to D3D11, remove this. crbug.com/438691
      GLenum format =
          video_decode_accelerator_.get()->GetSurfaceInternalFormat();
      if (format != GL_RGBA) {
        texture_manager->SetLevelInfo(texture_ref, texture_target_, 0, format,
                                      width, height, 1, 0, format, 0,
                                      gfx::Rect());
      }
    }
    buffers.push_back(media::PictureBuffer(buffer_ids[i], texture_dimensions_,
                                           texture_ref->service_id(),
                                           texture_ids[i]));
    textures.push_back(texture_ref);
  }
  video_decode_accelerator_->AssignPictureBuffers(buffers);
  DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
  for (uint32_t i = 0; i < buffer_ids.size(); ++i)
    uncleared_textures_[buffer_ids[i]] = textures[i];
}

void GpuVideoDecodeAccelerator::OnReusePictureBuffer(
    int32_t picture_buffer_id) {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->ReusePictureBuffer(picture_buffer_id);
}

void GpuVideoDecodeAccelerator::OnFlush() {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->Flush();
}

void GpuVideoDecodeAccelerator::OnReset() {
  DCHECK(video_decode_accelerator_);
  video_decode_accelerator_->Reset();
}

void GpuVideoDecodeAccelerator::OnDestroy() {
  DCHECK(video_decode_accelerator_);
  OnWillDestroyStub();
}

void GpuVideoDecodeAccelerator::OnFilterRemoved() {
  // We're destroying; cancel all callbacks.
  weak_factory_for_io_.InvalidateWeakPtrs();
  filter_removed_.Signal();
}

void GpuVideoDecodeAccelerator::SetTextureCleared(
    const media::Picture& picture) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DebugAutoLock auto_lock(debug_uncleared_textures_lock_);
  std::map<int32_t, scoped_refptr<gpu::gles2::TextureRef>>::iterator it;
  it = uncleared_textures_.find(picture.picture_buffer_id());
  if (it == uncleared_textures_.end())
    return;  // the texture has been cleared

  scoped_refptr<gpu::gles2::TextureRef> texture_ref = it->second;
  GLenum target = texture_ref->texture()->target();
  gpu::gles2::TextureManager* texture_manager =
      stub_->decoder()->GetContextGroup()->texture_manager();
  DCHECK(!texture_ref->texture()->IsLevelCleared(target, 0));
  texture_manager->SetLevelCleared(texture_ref.get(), target, 0, true);
  uncleared_textures_.erase(it);
}

void GpuVideoDecodeAccelerator::SendCreateDecoderReply(IPC::Message* message,
                                                       bool succeeded) {
  GpuCommandBufferMsg_CreateVideoDecoder::WriteReplyParams(message, succeeded);
  Send(message);
}

}  // namespace content
