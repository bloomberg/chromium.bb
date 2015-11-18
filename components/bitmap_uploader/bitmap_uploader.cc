// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bitmap_uploader/bitmap_uploader.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_surface.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/converters/surfaces/surfaces_utils.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace bitmap_uploader {
namespace {

const uint32_t g_transparent_color = 0x00000000;

void LostContext(void*) {
  DCHECK(false);
}

void OnGotContentHandlerID(uint32_t content_handler_id) {}

}  // namespace

BitmapUploader::BitmapUploader(mus::Window* window)
    : window_(window),
      color_(g_transparent_color),
      width_(0),
      height_(0),
      format_(BGRA),
      next_resource_id_(1u),
      id_namespace_(0u),
      local_id_(0u),
      surface_client_binding_(this) {}

BitmapUploader::~BitmapUploader() {
  MojoGLES2DestroyContext(gles2_context_);
}

void BitmapUploader::Init(mojo::Shell* shell) {
  surface_ = window_->RequestSurface(mus::mojom::SURFACE_TYPE_DEFAULT);
  surface_->BindToThread();

  mojo::ServiceProviderPtr gpu_service_provider;
  mojo::URLRequestPtr request2(mojo::URLRequest::New());
  request2->url = mojo::String::From("mojo:mus");
  shell->ConnectToApplication(request2.Pass(),
                              mojo::GetProxy(&gpu_service_provider), nullptr,
                              nullptr, base::Bind(&OnGotContentHandlerID));
  ConnectToService(gpu_service_provider.get(), &gpu_service_);

  mus::mojom::CommandBufferPtr gles2_client;
  gpu_service_->CreateOffscreenGLES2Context(GetProxy(&gles2_client));
  gles2_context_ = MojoGLES2CreateContext(
      gles2_client.PassInterface().PassHandle().release().value(),
      nullptr,
      &LostContext, NULL, mojo::Environment::GetDefaultAsyncWaiter());
  MojoGLES2MakeCurrent(gles2_context_);
}

// Sets the color which is RGBA.
void BitmapUploader::SetColor(uint32_t color) {
  if (color_ == color)
    return;
  color_ = color;
  if (surface_)
    Upload();
}

// Sets a bitmap.
void BitmapUploader::SetBitmap(int width,
                               int height,
                               scoped_ptr<std::vector<unsigned char>> data,
                               Format format) {
  width_ = width;
  height_ = height;
  bitmap_ = data.Pass();
  format_ = format;
  if (surface_)
    Upload();
}

void BitmapUploader::Upload() {
  const gfx::Rect bounds(window_->bounds());
  mus::mojom::PassPtr pass = mojo::CreateDefaultPass(1, bounds);
  mus::mojom::CompositorFramePtr frame = mus::mojom::CompositorFrame::New();

  // TODO(rjkroege): Support device scale factor in PDF viewer
  mus::mojom::CompositorFrameMetadataPtr meta =
      mus::mojom::CompositorFrameMetadata::New();
  meta->device_scale_factor = 1.0f;
  frame->metadata = meta.Pass();

  frame->resources.resize(0u);

  pass->quads.resize(0u);
  pass->shared_quad_states.push_back(
      mojo::CreateDefaultSQS(bounds.size()));

  MojoGLES2MakeCurrent(gles2_context_);
  if (bitmap_.get()) {
    mojo::Size bitmap_size;
    bitmap_size.width = width_;
    bitmap_size.height = height_;
    GLuint texture_id = BindTextureForSize(bitmap_size);
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    bitmap_size.width,
                    bitmap_size.height,
                    TextureFormat(),
                    GL_UNSIGNED_BYTE,
                    &((*bitmap_)[0]));

    GLbyte mailbox[GL_MAILBOX_SIZE_CHROMIUM];
    glGenMailboxCHROMIUM(mailbox);
    glProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox);
    gpu::SyncToken sync_token(glInsertSyncPointCHROMIUM());

    mus::mojom::TransferableResourcePtr resource =
        mus::mojom::TransferableResource::New();
    resource->id = next_resource_id_++;
    resource_to_texture_id_map_[resource->id] = texture_id;
    resource->format = mus::mojom::RESOURCE_FORMAT_RGBA_8888;
    resource->filter = GL_LINEAR;
    resource->size = bitmap_size.Clone();
    mus::mojom::MailboxHolderPtr mailbox_holder =
        mus::mojom::MailboxHolder::New();
    mailbox_holder->mailbox = mus::mojom::Mailbox::New();
    for (int i = 0; i < GL_MAILBOX_SIZE_CHROMIUM; ++i)
      mailbox_holder->mailbox->name.push_back(mailbox[i]);
    mailbox_holder->texture_target = GL_TEXTURE_2D;
    mailbox_holder->sync_token =
        mus::mojom::SyncToken::From<gpu::SyncToken>(sync_token);
    resource->mailbox_holder = mailbox_holder.Pass();
    resource->is_repeated = false;
    resource->is_software = false;

    mus::mojom::QuadPtr quad = mus::mojom::Quad::New();
    quad->material = mus::mojom::MATERIAL_TEXTURE_CONTENT;

    mojo::RectPtr rect = mojo::Rect::New();
    if (width_ <= bounds.width() && height_ <= bounds.height()) {
      rect->width = width_;
      rect->height = height_;
    } else {
      // The source bitmap is larger than the viewport. Resize it while
      // maintaining the aspect ratio.
      float width_ratio = static_cast<float>(width_) / bounds.width();
      float height_ratio = static_cast<float>(height_) / bounds.height();
      if (width_ratio > height_ratio) {
        rect->width = bounds.width();
        rect->height = height_ / width_ratio;
      } else {
        rect->height = bounds.height();
        rect->width = width_ / height_ratio;
      }
    }
    quad->rect = rect.Clone();
    quad->opaque_rect = rect.Clone();
    quad->visible_rect = rect.Clone();
    quad->needs_blending = true;
    quad->shared_quad_state_index = 0u;

    mus::mojom::TextureQuadStatePtr texture_state =
        mus::mojom::TextureQuadState::New();
    texture_state->resource_id = resource->id;
    texture_state->premultiplied_alpha = true;
    texture_state->uv_top_left = mojo::PointF::New();
    texture_state->uv_bottom_right = mojo::PointF::New();
    texture_state->uv_bottom_right->x = 1.f;
    texture_state->uv_bottom_right->y = 1.f;
    texture_state->background_color = mus::mojom::Color::New();
    texture_state->background_color->rgba = g_transparent_color;
    for (int i = 0; i < 4; ++i)
      texture_state->vertex_opacity.push_back(1.f);
    texture_state->y_flipped = false;

    frame->resources.push_back(resource.Pass());
    quad->texture_quad_state = texture_state.Pass();
    pass->quads.push_back(quad.Pass());
  }

  if (color_ != g_transparent_color) {
    mus::mojom::QuadPtr quad = mus::mojom::Quad::New();
    quad->material = mus::mojom::MATERIAL_SOLID_COLOR;
    quad->rect = mojo::Rect::From(bounds);
    quad->opaque_rect = mojo::Rect::New();
    quad->visible_rect = mojo::Rect::From(bounds);
    quad->needs_blending = true;
    quad->shared_quad_state_index = 0u;

    mus::mojom::SolidColorQuadStatePtr color_state =
        mus::mojom::SolidColorQuadState::New();
    color_state->color = mus::mojom::Color::New();
    color_state->color->rgba = color_;
    color_state->force_anti_aliasing_off = false;

    quad->solid_color_quad_state = color_state.Pass();
    pass->quads.push_back(quad.Pass());
  }

  frame->passes.push_back(pass.Pass());

  // TODO(rjkroege, fsamuel): We should throttle frames.
  surface_->SubmitCompositorFrame(frame.Pass(), mojo::Closure());
}

uint32_t BitmapUploader::BindTextureForSize(const mojo::Size size) {
  // TODO(jamesr): Recycle textures.
  GLuint texture = 0u;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D,
                0,
                TextureFormat(),
                size.width,
                size.height,
                0,
                TextureFormat(),
                GL_UNSIGNED_BYTE,
                0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  return texture;
}

void BitmapUploader::SetIdNamespace(uint32_t id_namespace) {
  id_namespace_ = id_namespace;
  if (color_ != g_transparent_color || bitmap_.get())
    Upload();
}

void BitmapUploader::ReturnResources(
    mojo::Array<mus::mojom::ReturnedResourcePtr> resources) {
  MojoGLES2MakeCurrent(gles2_context_);
  // TODO(jamesr): Recycle.
  for (size_t i = 0; i < resources.size(); ++i) {
    mus::mojom::ReturnedResourcePtr resource = resources[i].Pass();
    DCHECK_EQ(1, resource->count);
    glWaitSyncTokenCHROMIUM(
        resource->sync_token.To<gpu::SyncToken>().GetConstData());
    uint32_t texture_id = resource_to_texture_id_map_[resource->id];
    DCHECK_NE(0u, texture_id);
    resource_to_texture_id_map_.erase(resource->id);
    glDeleteTextures(1, &texture_id);
  }
}

}  // namespace bitmap_uploader
