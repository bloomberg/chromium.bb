// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_frame_factory_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "media/base/video_frame.h"
#include "media/gpu//android/codec_image.h"
#include "media/gpu/android/codec_wrapper.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace media {
namespace {

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

VideoFrameFactoryImpl::VideoFrameFactoryImpl()
    : gpu_video_frame_factory_(base::MakeUnique<GpuVideoFrameFactory>()) {}

VideoFrameFactoryImpl::~VideoFrameFactoryImpl() {
  gpu_task_runner_->DeleteSoon(FROM_HERE, gpu_video_frame_factory_.release());
}

void VideoFrameFactoryImpl::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetStubCb get_stub_cb,
    InitCb init_cb) {
  gpu_task_runner_ = std::move(gpu_task_runner);
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
    FrameCreatedCb frame_created_cb) {
  base::PostTaskAndReplyWithResult(
      gpu_task_runner_.get(), FROM_HERE,
      base::Bind(&GpuVideoFrameFactory::CreateVideoFrame,
                 base::Unretained(gpu_video_frame_factory_.get()),
                 base::Passed(&output_buffer), surface_texture, timestamp,
                 natural_size),
      std::move(frame_created_cb));
}

GpuVideoFrameFactory::GpuVideoFrameFactory() : weak_factory_(this) {}

GpuVideoFrameFactory::~GpuVideoFrameFactory() = default;

scoped_refptr<SurfaceTextureGLOwner> GpuVideoFrameFactory::Initialize(
    VideoFrameFactoryImpl::GetStubCb get_stub_cb) {
  auto* stub = std::move(get_stub_cb).Run();
  if (!stub)
    return nullptr;
  stub_ = stub->AsWeakPtr();

  auto* decoder = stub_->decoder();
  if (!decoder || !decoder->MakeCurrent())
    return nullptr;
  return SurfaceTextureGLOwnerImpl::Create();
}

scoped_refptr<VideoFrame> GpuVideoFrameFactory::CreateVideoFrame(
    std::unique_ptr<CodecOutputBuffer> output_buffer,
    scoped_refptr<SurfaceTextureGLOwner> surface_texture,
    base::TimeDelta timestamp,
    gfx::Size natural_size) {
  if (!stub_)
    return nullptr;

  // TODO(watk): Which of these null checks are unnecessary?
  gpu::gles2::GLES2Decoder* decoder = stub_->decoder();
  if (!decoder->MakeCurrent())
    return nullptr;
  gpu::gles2::ContextGroup* group = decoder->GetContextGroup();
  if (!group)
    return nullptr;
  gpu::gles2::TextureManager* texture_manager = group->texture_manager();
  if (!texture_manager)
    return nullptr;
  gpu::GpuChannel* channel = stub_->channel();
  if (!channel)
    return nullptr;
  gpu::SyncPointManager* sync_point_manager = channel->sync_point_manager();
  if (!sync_point_manager)
    return nullptr;
  gpu::gles2::MailboxManager* mailbox_manager = group->mailbox_manager();
  if (!mailbox_manager)
    return nullptr;

  // Create a new CodecImage to back the video frame and try to render it early.
  auto image = make_scoped_refptr(
      new CodecImage(std::move(output_buffer), surface_texture,
                     base::Bind(&GpuVideoFrameFactory::OnImageDestructed,
                                weak_factory_.GetWeakPtr())));
  images_.push_back(image.get());
  internal::MaybeRenderEarly(&images_);

  // Create a new Texture with the image attached and put it into a mailbox
  gfx::Size size = output_buffer->size();
  auto texture_ref =
      CreateTexture(decoder, texture_manager, std::move(image), size);
  // TODO(watk): The TextureRef will be deleted at the end of this scope because
  // mailboxes don't keep a strong ref. A coming change to
  // MojoVideoDecoderService will deliver a callback when the VideoFrame is
  // destroyed by the client. So we can simply bind the ref into the callback,
  // along with a thread hop to ensure the ref is dropped on this thread.
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  mailbox_manager->ProduceTexture(mailbox, texture_ref->texture());

  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::GPU_IO,  // TODO(sandersd): Is this right?
      stub_->stream_id(), stub_->command_buffer_id(),
      0);  // TODO(sandersd): Current release count?
  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  mailbox_holders[0] =
      gpu::MailboxHolder(mailbox, sync_token, GL_TEXTURE_EXTERNAL_OES);

  gfx::Rect visible_rect(size);
  // Note: The pixel format doesn't matter.
  return VideoFrame::WrapNativeTextures(PIXEL_FORMAT_ARGB, mailbox_holders,
                                        VideoFrame::ReleaseMailboxCB(), size,
                                        visible_rect, natural_size, timestamp);
}

void GpuVideoFrameFactory::OnImageDestructed(CodecImage* image) {
  base::Erase(images_, image);
  internal::MaybeRenderEarly(&images_);
}

}  // namespace media
