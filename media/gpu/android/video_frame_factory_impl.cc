// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/gpu//android/codec_image.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace media {
namespace {

bool MakeContextCurrent(gpu::GpuCommandBufferStub* stub) {
  return stub && stub->decoder()->MakeCurrent();
}

// Creates a new EXTERNAL_OES Texture with |image| attached, and returns the
// TextureRef.
// TODO(watk): Refactor this when CommandBufferHelper lands
// (http://crrev.com/2938543005)
scoped_refptr<gpu::gles2::TextureRef> CreateTexture(
    gpu::gles2::GLES2Decoder* decoder,
    gpu::gles2::TextureManager* texture_manager,
    scoped_refptr<CodecImage> image,
    gfx::Size size) {
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  // Rather than calling texture_manager->CreateTexture(), create the
  // Texture manually to avoid registering a client_id (because this texture
  // won't have one).
  scoped_refptr<gpu::gles2::TextureRef> texture_ref =
      gpu::gles2::TextureRef::Create(texture_manager, 0, texture_id);
  texture_manager->SetTarget(texture_ref.get(), GL_TEXTURE_EXTERNAL_OES);
  const char* caller = "GpuVideoFrameFactory::CreateTexture";
  texture_manager->SetLevelInfo(texture_ref.get(),
                                GL_TEXTURE_EXTERNAL_OES,  // target
                                0,                        // level
                                GL_RGBA,                  // internal_format
                                size.width(),             // width
                                size.height(),            // height
                                1,                        // depth
                                0,                        // border
                                GL_RGBA,                  // format
                                GL_UNSIGNED_BYTE,         // type
                                gfx::Rect(size));         // cleared_rect
  texture_manager->SetParameteri(caller, decoder->GetErrorState(),
                                 texture_ref.get(), GL_TEXTURE_MAG_FILTER,
                                 GL_LINEAR);
  texture_manager->SetParameteri(caller, decoder->GetErrorState(),
                                 texture_ref.get(), GL_TEXTURE_MIN_FILTER,
                                 GL_LINEAR);
  texture_manager->SetParameteri(caller, decoder->GetErrorState(),
                                 texture_ref.get(), GL_TEXTURE_WRAP_S,
                                 GL_CLAMP_TO_EDGE);
  texture_manager->SetParameteri(caller, decoder->GetErrorState(),
                                 texture_ref.get(), GL_TEXTURE_WRAP_T,
                                 GL_CLAMP_TO_EDGE);
  texture_manager->SetParameteri(caller, decoder->GetErrorState(),
                                 texture_ref.get(), GL_TEXTURE_BASE_LEVEL, 0);
  texture_manager->SetParameteri(caller, decoder->GetErrorState(),
                                 texture_ref.get(), GL_TEXTURE_MAX_LEVEL, 0);

  // If we're attaching a SurfaceTexture backed image, we set the state to
  // UNBOUND. This ensures that the implementation will call CopyTexImage()
  // which lets us update the surface texture at the right time.
  // For overlays we set the state to BOUND because it's required for
  // ScheduleOverlayPlane() to be called. If something tries to sample from an
  // overlay texture it won't work, but there's no way to make that work.
  auto surface_texture = image->surface_texture();
  auto image_state = surface_texture ? gpu::gles2::Texture::UNBOUND
                                     : gpu::gles2::Texture::BOUND;
  GLuint surface_texture_service_id =
      surface_texture ? surface_texture->GetTextureId() : 0;
  texture_manager->SetLevelStreamTextureImage(
      texture_ref.get(), GL_TEXTURE_EXTERNAL_OES, 0, image.get(), image_state,
      surface_texture_service_id);
  texture_manager->SetLevelCleared(texture_ref.get(), GL_TEXTURE_EXTERNAL_OES,
                                   0, true);
  return texture_ref;
}

}  // namespace

VideoFrameFactoryImpl::VideoFrameFactoryImpl() = default;

VideoFrameFactoryImpl::~VideoFrameFactoryImpl() {
  if (gpu_video_frame_factory_)
    gpu_task_runner_->DeleteSoon(FROM_HERE, gpu_video_frame_factory_.release());
}

void VideoFrameFactoryImpl::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetStubCb get_stub_cb,
    InitCb init_cb) {
  gpu_task_runner_ = std::move(gpu_task_runner);
  gpu_video_frame_factory_ = base::MakeUnique<GpuVideoFrameFactory>();
  base::PostTaskAndReplyWithResult(
      gpu_task_runner_.get(), FROM_HERE,
      base::Bind(&GpuVideoFrameFactory::Initialize,
                 base::Unretained(gpu_video_frame_factory_.get()), get_stub_cb),
      std::move(init_cb));
}

void VideoFrameFactoryImpl::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<SurfaceTextureGLOwner> surface_texture,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    OutputWithReleaseMailboxCB output_cb) {
  gpu_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuVideoFrameFactory::CreateVideoFrame,
                            base::Unretained(gpu_video_frame_factory_.get()),
                            base::Passed(&output_buffer), surface_texture,
                            timestamp, natural_size, std::move(output_cb),
                            base::ThreadTaskRunnerHandle::Get()));
}

GpuVideoFrameFactory::GpuVideoFrameFactory() : weak_factory_(this) {}

GpuVideoFrameFactory::~GpuVideoFrameFactory() {
  if (stub_)
    stub_->RemoveDestructionObserver(this);
  ClearTextureRefs();
}

scoped_refptr<SurfaceTextureGLOwner> GpuVideoFrameFactory::Initialize(
    VideoFrameFactoryImpl::GetStubCb get_stub_cb) {
  stub_ = get_stub_cb.Run();
  if (!MakeContextCurrent(stub_))
    return nullptr;
  stub_->AddDestructionObserver(this);
  return SurfaceTextureGLOwnerImpl::Create();
}

void GpuVideoFrameFactory::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<SurfaceTextureGLOwner> surface_texture,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    VideoFrameFactory::OutputWithReleaseMailboxCB output_cb,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  scoped_refptr<VideoFrame> frame;
  scoped_refptr<gpu::gles2::TextureRef> texture_ref;
  CreateVideoFrameInternal(std::move(output_buffer), std::move(surface_texture),
                           timestamp, natural_size, &frame, &texture_ref);
  if (!frame || !texture_ref)
    return;

  // TODO(sandersd, watk): The VideoFrame release callback will not be called
  // after MojoVideoDecoderService is destructed, so we have to release all
  // our TextureRefs when |this| is destructed. This can unback outstanding
  // VideoFrames (e.g., the current frame when the player is suspended). The
  // release callback lifetime should be separate from MCVD or
  // MojoVideoDecoderService (http://crbug.com/737220).
  texture_refs_[texture_ref.get()] = texture_ref;
  auto release_cb = BindToCurrentLoop(base::Bind(
      &GpuVideoFrameFactory::DropTextureRef, weak_factory_.GetWeakPtr(),
      base::Unretained(texture_ref.get())));
  task_runner->PostTask(FROM_HERE, base::Bind(output_cb, std::move(release_cb),
                                              std::move(frame)));
}

void GpuVideoFrameFactory::CreateVideoFrameInternal(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<SurfaceTextureGLOwner> surface_texture,
    base::TimeDelta timestamp,
    gfx::Size natural_size,
    scoped_refptr<VideoFrame>* video_frame_out,
    scoped_refptr<gpu::gles2::TextureRef>* texture_ref_out) {
  if (!MakeContextCurrent(stub_))
    return;

  // TODO(watk): Which of these null checks are unnecessary?
  gpu::gles2::ContextGroup* group = stub_->decoder()->GetContextGroup();
  if (!group)
    return;
  gpu::gles2::TextureManager* texture_manager = group->texture_manager();
  if (!texture_manager)
    return;
  gpu::GpuChannel* channel = stub_->channel();
  if (!channel)
    return;
  gpu::SyncPointManager* sync_point_manager = channel->sync_point_manager();
  if (!sync_point_manager)
    return;
  gpu::gles2::MailboxManager* mailbox_manager = group->mailbox_manager();
  if (!mailbox_manager)
    return;

  gfx::Size size = output_buffer->size();
  gfx::Rect visible_rect(size);
  // Check that we can create a VideoFrame for this config before creating the
  // TextureRef so that we don't have to clean up the TextureRef if creating the
  // frame fails.
  if (!VideoFrame::IsValidConfig(PIXEL_FORMAT_ARGB, VideoFrame::STORAGE_OPAQUE,
                                 size, visible_rect, natural_size)) {
    return;
  }

  // Create a new CodecImage to back the video frame and try to render it early.
  auto image = make_scoped_refptr(
      new CodecImage(std::move(output_buffer), surface_texture,
                     base::Bind(&GpuVideoFrameFactory::OnImageDestructed,
                                weak_factory_.GetWeakPtr())));
  images_.push_back(image.get());
  internal::MaybeRenderEarly(&images_);

  // Create a new Texture with the image attached and put it into a mailbox
  auto texture_ref =
      CreateTexture(stub_->decoder(), texture_manager, std::move(image), size);
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  mailbox_manager->ProduceTexture(mailbox, texture_ref->texture());

  // TODO(watk): Remove this once it's possible to leave the mailbox
  // sync token empty (http://crrev.com/c/548973).
  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            stub_->stream_id(), stub_->command_buffer_id(), 0);
  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  mailbox_holders[0] =
      gpu::MailboxHolder(mailbox, sync_token, GL_TEXTURE_EXTERNAL_OES);

  // Note: The pixel format doesn't matter.
  auto frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_ARGB, mailbox_holders, VideoFrame::ReleaseMailboxCB(), size,
      visible_rect, natural_size, timestamp);

  *video_frame_out = std::move(frame);
  *texture_ref_out = std::move(texture_ref);
}

void GpuVideoFrameFactory::OnWillDestroyStub() {
  DCHECK(stub_);
  ClearTextureRefs();
  stub_ = nullptr;
}

void GpuVideoFrameFactory::ClearTextureRefs() {
  DCHECK(stub_ || texture_refs_.empty());
  // If we fail to make the context current, we have to notify the TextureRefs
  // so they don't try to delete textures without a context.
  if (!MakeContextCurrent(stub_)) {
    for (const auto& kv : texture_refs_)
      kv.first->ForceContextLost();
  }
  texture_refs_.clear();
}

void GpuVideoFrameFactory::DropTextureRef(gpu::gles2::TextureRef* ref,
                                          const gpu::SyncToken& token) {
  auto it = texture_refs_.find(ref);
  if (it == texture_refs_.end())
    return;
  // If we fail to make the context current, we have to notify the TextureRef
  // so it doesn't try to delete a texture without a context.
  if (!MakeContextCurrent(stub_))
    ref->ForceContextLost();
  texture_refs_.erase(it);
}

void GpuVideoFrameFactory::OnImageDestructed(CodecImage* image) {
  base::Erase(images_, image);
  internal::MaybeRenderEarly(&images_);
}

}  // namespace media
