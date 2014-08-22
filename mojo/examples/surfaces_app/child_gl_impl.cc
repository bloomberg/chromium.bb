// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/surfaces_app/child_gl_impl.h"

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/texture_draw_quad.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "mojo/examples/surfaces_app/surfaces_util.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"
#include "mojo/services/public/interfaces/surfaces/surface_id.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace mojo {
namespace examples {

using cc::RenderPass;
using cc::RenderPassId;
using cc::DrawQuad;
using cc::TextureDrawQuad;
using cc::DelegatedFrameData;
using cc::CompositorFrame;

static void ContextLostThunk(void*) {
  LOG(FATAL) << "Context lost";
}

ChildGLImpl::ChildGLImpl(ApplicationConnection* surfaces_service_connection,
                         CommandBufferPtr command_buffer)
    : start_time_(base::TimeTicks::Now()), next_resource_id_(1) {
  surfaces_service_connection->ConnectToService(&surface_);
  surface_.set_client(this);
  context_ =
      MojoGLES2CreateContext(command_buffer.PassMessagePipe().release().value(),
                             &ContextLostThunk,
                             this,
                             Environment::GetDefaultAsyncWaiter());
  DCHECK(context_);
  MojoGLES2MakeCurrent(context_);
}

ChildGLImpl::~ChildGLImpl() {
  MojoGLES2DestroyContext(context_);
  surface_->DestroySurface(mojo::SurfaceId::From(id_));
}

void ChildGLImpl::ProduceFrame(
    ColorPtr color,
    SizePtr size,
    const mojo::Callback<void(SurfaceIdPtr id)>& callback) {
  color_ = color.To<SkColor>();
  size_ = size.To<gfx::Size>();
  cube_.Init(size_.width(), size_.height());
  cube_.set_color(
      SkColorGetR(color_), SkColorGetG(color_), SkColorGetB(color_));
  produce_callback_ = callback;
  if (allocator_)
    AllocateSurface();
}

void ChildGLImpl::SetIdNamespace(uint32_t id_namespace) {
  allocator_.reset(new cc::SurfaceIdAllocator(id_namespace));
  if (!produce_callback_.is_null())
    AllocateSurface();
}

void ChildGLImpl::ReturnResources(Array<ReturnedResourcePtr> resources) {
  for (size_t i = 0; i < resources.size(); ++i) {
    cc::ReturnedResource res = resources[i].To<cc::ReturnedResource>();
    GLuint returned_texture = id_to_tex_map_[res.id];
    glDeleteTextures(1, &returned_texture);
  }
}

void ChildGLImpl::AllocateSurface() {
  if (produce_callback_.is_null() || !allocator_)
    return;

  id_ = allocator_->GenerateId();
  surface_->CreateSurface(mojo::SurfaceId::From(id_), mojo::Size::From(size_));
  produce_callback_.Run(SurfaceId::From(id_));
  Draw();
}

void ChildGLImpl::Draw() {
  // First, generate a GL texture and draw the cube into it.
  GLuint texture = 0u;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               size_.width(),
               size_.height(),
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  GLuint fbo = 0u;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  DCHECK_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));
  glClearColor(1, 0, 0, 0.5);
  cube_.UpdateForTimeDelta(0.16f);
  cube_.Draw();

  // Then, put the texture into a mailbox.
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  glProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  GLuint sync_point = glInsertSyncPointCHROMIUM();
  gpu::MailboxHolder holder(mailbox, GL_TEXTURE_2D, sync_point);

  // Then, put the mailbox into a TransferableResource
  cc::TransferableResource resource;
  resource.id = next_resource_id_++;
  id_to_tex_map_[resource.id] = texture;
  resource.format = cc::RGBA_8888;
  resource.filter = GL_LINEAR;
  resource.size = size_;
  resource.mailbox_holder = holder;
  resource.is_repeated = false;
  resource.is_software = false;

  gfx::Rect rect(size_);
  RenderPassId id(1, 1);
  scoped_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetNew(id, rect, rect, gfx::Transform());

  CreateAndAppendSimpleSharedQuadState(pass.get(), gfx::Transform(), size_);

  TextureDrawQuad* texture_quad =
      pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  float vertex_opacity[4] = {1.0f, 1.0f, 0.2f, 1.0f};
  texture_quad->SetNew(pass->shared_quad_state_list.back(),
                       rect,
                       rect,
                       rect,
                       resource.id,
                       true,
                       gfx::PointF(),
                       gfx::PointF(1.f, 1.f),
                       SK_ColorBLUE,
                       vertex_opacity,
                       false);

  scoped_ptr<DelegatedFrameData> delegated_frame_data(new DelegatedFrameData);
  delegated_frame_data->render_pass_list.push_back(pass.Pass());
  delegated_frame_data->resource_list.push_back(resource);

  scoped_ptr<CompositorFrame> frame(new CompositorFrame);
  frame->delegated_frame_data = delegated_frame_data.Pass();

  surface_->SubmitFrame(mojo::SurfaceId::From(id_), mojo::Frame::From(*frame));

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ChildGLImpl::Draw, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(50));
}

}  // namespace examples
}  // namespace mojo
