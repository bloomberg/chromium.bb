// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_view_x.h"

#include "base/scoped_ptr.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_backing_store_glx.h"
#include "chrome/gpu/gpu_backing_store_glx_context.h"
#include "chrome/gpu/gpu_thread.h"

// X stuff must be last since it does "#define Status int" which messes up some
// of the header files we indirectly pull in.
#include <GL/glxew.h>
#include <X11/Xutil.h>

GpuViewX::GpuViewX(GpuThread* gpu_thread,
                   XID parent,
                   int32 routing_id)
    : gpu_thread_(gpu_thread),
      routing_id_(routing_id),
      window_(parent) {
  gpu_thread_->AddRoute(routing_id_, this);
}

GpuViewX::~GpuViewX() {
  gpu_thread_->RemoveRoute(routing_id_);
  // TODO(brettw) may want to delete any dangling backing stores, or perhaps
  // assert if one still exists.
}

GLXContext GpuViewX::BindContext() {
  GLXContext ctx = gpu_thread_->GetGLXContext()->BindContext(window_);
  CHECK(ctx);
  return ctx;
}

void GpuViewX::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(GpuViewX, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_NewBackingStore, OnNewBackingStore)
    IPC_MESSAGE_HANDLER(GpuMsg_WindowPainted, OnWindowPainted)
  IPC_END_MESSAGE_MAP_EX()
}

void GpuViewX::OnChannelConnected(int32 peer_pid) {
}

void GpuViewX::OnChannelError() {
  // TODO(brettw) do we need to delete ourselves now?
}

void GpuViewX::DidScrollBackingStoreRect(int dx, int dy,
                                         const gfx::Rect& rect) {
}

void GpuViewX::Repaint() {
  BindContext();

  const gfx::Size& size = backing_store_->size();

  glViewport(0, 0, size.width(), size.height());

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, backing_store_->texture_id());

  // TODO(brettw) use vertex buffers.
  // TODO(brettw) make this so we use the texture size rather than the whole
  // area size so we don't stretch bitmaps.
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0, 1.0);

    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-1.0, -1.0);

    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0, -1.0);

    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(1.0, 1.0);
  glEnd();
  DCHECK(glGetError() == GL_NO_ERROR);

  // TODO(brettw) when we no longer stretch non-fitting bitmaps, we should
  // paint white over any unpainted area here.

  glXSwapBuffers(gpu_thread_->display(), window_);
}

void GpuViewX::OnNewBackingStore(int32 routing_id, const gfx::Size& size) {
  backing_store_.reset(
      new GpuBackingStoreGLX(this, gpu_thread_, routing_id, size));
}

void GpuViewX::OnWindowPainted() {
  Repaint();
}
