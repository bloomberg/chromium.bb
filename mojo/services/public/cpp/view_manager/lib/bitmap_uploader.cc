// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/bitmap_uploader.h"

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif  // GL_GLEXT_PROTOTYPES

#include "base/bind.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_utils.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_client_impl.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {

namespace {
void LostContext(void*) {
  DCHECK(false);
}

uint32_t TextureFormat() {
  return SK_B32_SHIFT ? GL_RGBA : GL_BGRA_EXT;
}
}

BitmapUploader::BitmapUploader(ViewManagerClientImpl* client,
                               Id view_id,
                               SurfacesServicePtr surfaces_service,
                               GpuPtr gpu_service)
    : client_(client),
      view_id_(view_id),
      surfaces_service_(surfaces_service.Pass()),
      gpu_service_(gpu_service.Pass()),
      color_(SK_ColorTRANSPARENT),
      next_resource_id_(1u),
      weak_factory_(this) {
  surfaces_service_->CreateSurfaceConnection(base::Bind(
      &BitmapUploader::OnSurfaceConnectionCreated, weak_factory_.GetWeakPtr()));
  CommandBufferPtr gles2_client;
  gpu_service_->CreateOffscreenGLES2Context(Get(&gles2_client));
  gles2_context_ =
      MojoGLES2CreateContext(gles2_client.PassMessagePipe().release().value(),
                             &LostContext,
                             NULL,
                             Environment::GetDefaultAsyncWaiter());
  MojoGLES2MakeCurrent(gles2_context_);
}

BitmapUploader::~BitmapUploader() {
  MojoGLES2DestroyContext(gles2_context_);
}

void BitmapUploader::SetSize(const gfx::Size& size) {
  if (size_ == size)
    return;
  size_ = size;
}

void BitmapUploader::SetColor(SkColor color) {
  if (color_ == color)
    return;
  color_ = color;
  if (surface_)
    Upload();
}

void BitmapUploader::SetBitmap(SkBitmap bitmap) {
  bitmap_ = bitmap;
  if (surface_)
    Upload();
}

void BitmapUploader::Upload() {
  if (size_.width() == 0 || size_.height() == 0) {
    client_->SetSurfaceId(view_id_, SurfaceId::New());
    return;
  }
  if (!surface_) {  // Can't upload yet, store for later.
    done_callback_ = done_callback_;
    return;
  }
  if (id_.is_null() || size_ != surface_size_) {
    if (!id_.is_null())
      surface_->DestroySurface(SurfaceId::From(id_));
    id_ = id_allocator_->GenerateId();
    surface_->CreateSurface(SurfaceId::From(id_), Size::From(size_));
    client_->SetSurfaceId(view_id_, SurfaceId::From(id_));
    surface_size_ = size_;
  }

  gfx::Rect bounds(size_);
  PassPtr pass = CreateDefaultPass(1, bounds);
  FramePtr frame = Frame::New();
  frame->resources.resize(0u);

  pass->quads.resize(0u);
  pass->shared_quad_states.push_back(CreateDefaultSQS(size_));

  MojoGLES2MakeCurrent(gles2_context_);
  if (!bitmap_.isNull()) {
    gfx::Size bitmap_size(bitmap_.width(), bitmap_.height());
    GLuint texture_id = BindTextureForSize(bitmap_size);
    bitmap_.lockPixels();
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    bitmap_size.width(),
                    bitmap_size.height(),
                    TextureFormat(),
                    GL_UNSIGNED_BYTE,
                    bitmap_.getPixels());
    bitmap_.unlockPixels();

    gpu::Mailbox mailbox = gpu::Mailbox::Generate();
    glProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    GLuint sync_point = glInsertSyncPointCHROMIUM();

    TransferableResourcePtr resource = TransferableResource::New();
    resource->id = next_resource_id_++;
    resource_to_texture_id_map_[resource->id] = texture_id;
    resource->format = mojo::RESOURCE_FORMAT_RGBA_8888;
    resource->filter = GL_LINEAR;
    resource->size = Size::From(bitmap_size);
    MailboxHolderPtr mailbox_holder = MailboxHolder::New();
    mailbox_holder->mailbox = Mailbox::From(mailbox);
    mailbox_holder->texture_target = GL_TEXTURE_2D;
    mailbox_holder->sync_point = sync_point;
    resource->mailbox_holder = mailbox_holder.Pass();
    resource->is_repeated = false;
    resource->is_software = false;

    QuadPtr quad = Quad::New();
    quad->material = MATERIAL_TEXTURE_CONTENT;
    quad->rect = Rect::From(bounds);
    quad->opaque_rect = Rect::From(bounds);
    quad->visible_rect = Rect::From(bounds);
    quad->needs_blending = true;
    quad->shared_quad_state_index = 0u;

    TextureQuadStatePtr texture_state = TextureQuadState::New();
    texture_state->resource_id = resource->id;
    texture_state->premultiplied_alpha = true;
    texture_state->uv_top_left = PointF::From(gfx::PointF(0.f, 0.f));
    texture_state->uv_bottom_right = PointF::From(gfx::PointF(1.f, 1.f));
    texture_state->background_color = Color::From(SkColor(SK_ColorTRANSPARENT));
    for (int i = 0; i < 4; ++i)
      texture_state->vertex_opacity.push_back(1.f);
    texture_state->flipped = false;

    frame->resources.push_back(resource.Pass());
    quad->texture_quad_state = texture_state.Pass();
    pass->quads.push_back(quad.Pass());
  }

  if (color_ != SK_ColorTRANSPARENT) {
    QuadPtr quad = Quad::New();
    quad->material = MATERIAL_SOLID_COLOR;
    quad->rect = Rect::From(bounds);
    quad->opaque_rect = Rect::From(gfx::Rect());
    quad->visible_rect = Rect::From(bounds);
    quad->needs_blending = true;
    quad->shared_quad_state_index = 0u;

    SolidColorQuadStatePtr color_state = SolidColorQuadState::New();
    color_state->color = Color::From(color_);
    color_state->force_anti_aliasing_off = false;

    quad->solid_color_quad_state = color_state.Pass();
    pass->quads.push_back(quad.Pass());
  }

  frame->passes.push_back(pass.Pass());

  surface_->SubmitFrame(SurfaceId::From(id_), frame.Pass());
}

void BitmapUploader::ReturnResources(Array<ReturnedResourcePtr> resources) {
  if (!resources.size())
    return;
  MojoGLES2MakeCurrent(gles2_context_);
  // TODO(jamesr): Recycle.
  for (size_t i = 0; i < resources.size(); ++i) {
    ReturnedResourcePtr resource = resources[i].Pass();
    DCHECK_EQ(1, resource->count);
    glWaitSyncPointCHROMIUM(resource->sync_point);
    uint32_t texture_id = resource_to_texture_id_map_[resource->id];
    DCHECK_NE(0u, texture_id);
    resource_to_texture_id_map_.erase(resource->id);
    glDeleteTextures(1, &texture_id);
  }
}

void BitmapUploader::OnSurfaceConnectionCreated(SurfacePtr surface,
                                                uint32_t id_namespace) {
  surface_ = surface.Pass();
  surface_.set_client(this);
  id_allocator_.reset(new cc::SurfaceIdAllocator(id_namespace));
  if (color_ != SK_ColorTRANSPARENT || !bitmap_.isNull())
    Upload();
}

uint32_t BitmapUploader::BindTextureForSize(const gfx::Size size) {
  // TODO(jamesr): Recycle textures.
  GLuint texture = 0u;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               TextureFormat(),
               size.width(),
               size.height(),
               0,
               TextureFormat(),
               GL_UNSIGNED_BYTE,
               0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  return texture;
}

}  // namespace mojo
