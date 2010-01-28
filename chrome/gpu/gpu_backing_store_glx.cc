// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_backing_store_glx.h"

#include <GL/glew.h>

#include "base/scoped_ptr.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/transport_dib.h"
#include "chrome/gpu/gpu_backing_store_glx_context.h"
#include "chrome/gpu/gpu_thread.h"
#include "chrome/gpu/gpu_view_x.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"

GpuBackingStoreGLX::GpuBackingStoreGLX(GpuViewX* view,
                                       GpuThread* gpu_thread,
                                       int32 routing_id,
                                       const gfx::Size& size)
    : view_(view),
      gpu_thread_(gpu_thread),
      routing_id_(routing_id),
      size_(size),
      texture_id_(0) {
  gpu_thread_->AddRoute(routing_id_, this);

  view_->BindContext();  // Must do this before issuing OpenGl.

  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

GpuBackingStoreGLX::~GpuBackingStoreGLX() {
  if (texture_id_)
    glDeleteTextures(1, &texture_id_);
  gpu_thread_->RemoveRoute(routing_id_);
}

void GpuBackingStoreGLX::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(GpuBackingStoreGLX, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_PaintToBackingStore, OnPaintToBackingStore)
    IPC_MESSAGE_HANDLER(GpuMsg_ScrollBackingStore, OnScrollBackingStore)
  IPC_END_MESSAGE_MAP_EX()
}

void GpuBackingStoreGLX::OnChannelConnected(int32 peer_pid) {
}

void GpuBackingStoreGLX::OnChannelError() {
  // FIXME(brettw) does this mean we aren't getting any more messages and we
  // should delete outselves?
  NOTIMPLEMENTED();
}

void GpuBackingStoreGLX::OnPaintToBackingStore(
    base::ProcessId source_process_id,
    TransportDIB::Id id,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects) {
  TransportDIB* dib = TransportDIB::Map(id);
  view_->BindContext();

  scoped_ptr<skia::PlatformCanvas> canvas(
      dib->GetPlatformCanvas(bitmap_rect.width(), bitmap_rect.height()));
  const SkBitmap& transport_bitmap =
      canvas->getTopPlatformDevice().accessBitmap(false);

  for (size_t i = 0; i < copy_rects.size(); i++)
    PaintOneRectToBackingStore(transport_bitmap, bitmap_rect, copy_rects[i]);

  gpu_thread_->Send(new GpuHostMsg_PaintToBackingStore_ACK(routing_id_));

  view_->Repaint();
}

void GpuBackingStoreGLX::OnScrollBackingStore(int dx, int dy,
                                              const gfx::Rect& clip_rect,
                                              const gfx::Size& view_size) {
  // Create a framebuffer to render our scrolled texture into.
  GpuBackingStoreGLXContext* context = view_->gpu_thread()->GetGLXContext();
  if (!context->BindTextureForScrolling(view_->window(), size_))
    return;

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture_id_);

  // Set up the the tranform so we can paint exact pixels to the screen, with
  // (0, 0) at the bottom left.
  float w = static_cast<float>(size_.width());
  float h = static_cast<float>(size_.height());
  glViewport(0, 0, size_.width(), size_.height());
  glLoadIdentity();
  glOrtho(0.0, w, 0.0, h, -1.0, 1.0);

  // Paint the non-scrolled background of the page. Note that we try to avoid
  // this if the entire thing is scrolling, which is a common case.
  if (view_size != clip_rect.size()) {
    glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 0.0f);
      glVertex2f(0.0, 0.0);

      glTexCoord2f(0.0f, 1.0f);
      glVertex2f(0.0, h);

      glTexCoord2f(1.0f, 1.0f);
      glVertex2f(w, h);

      glTexCoord2f(1.0f, 0.0f);
      glVertex2f(w, 0.0);
    glEnd();
  }

  // Constrain the painting to only the area we're scrolling.  Compute the clip
  // rect in OpenGL pixel coords, which uses the lower-left as the origin.
  gfx::Rect gl_clip_rect(clip_rect.x(), clip_rect.y(),
                         clip_rect.width(), clip_rect.height());

  glEnable(GL_SCISSOR_TEST);
  glScissor(gl_clip_rect.x(), gl_clip_rect.y(),
            gl_clip_rect.width(), gl_clip_rect.height());

  // Paint the offset texture.
  glTranslatef(static_cast<float>(dx), static_cast<float>(dy), 0.0f);
  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0, 0.0);

    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0, h);

    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(w, h);

    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(w, 0.0);
  glEnd();
  glDisable(GL_SCISSOR_TEST);

  glBindTexture(GL_TEXTURE_2D, 0);
  texture_id_ = context->SwapTextureForScrolling(texture_id_, size_);
  glFlush();
  DCHECK(texture_id_);
}

void GpuBackingStoreGLX::PaintOneRectToBackingStore(
    const SkBitmap& transport_bitmap,
    const gfx::Rect& bitmap_rect,
    const gfx::Rect& copy_rect) {
  // Make a bitmap referring to the correct subset of the input bitmap.
  SkBitmap copy_bitmap;
  if (copy_rect.x() == 0 &&
      copy_rect.y() == 0 &&
      copy_rect.width() == bitmap_rect.width() &&
      copy_rect.height() == bitmap_rect.height()) {
    // The subregion we're being asked to copy is the full bitmap. We don't
    // have to do any extra work to make the bitmap, we can just refer to the
    // original data (bitmap assignments are just refs to the original).
    copy_bitmap = transport_bitmap;
  } else {
    // Make a rect referring to the subset into the original (copy_rect and
    // bitmap_rect are both in global coords) and make a copy of that data to
    // give to OpenGL later.
    //
    // TODO(brettw) on desktop GL (not ES) we can do a trick here using
    // GL_UNPACK_ROW_WIDTH, GL_UNPACK_SKIP_PIXELS and GL_UNPACK_SKIP_ROWS to
    // avoid this copy.
    //
    // On ES, it may be better to actually call subimage for each row of
    // the updated bitmap to avoid the copy. We will have to benchmark that
    // approach against making the copy here to see if it performs better on
    // the systems we're targeting.
    SkIRect sk_copy_rect = { copy_rect.x() - bitmap_rect.x(),
                             copy_rect.y() - bitmap_rect.y(),
                             copy_rect.right() - bitmap_rect.x(),
                             copy_rect.bottom() - bitmap_rect.y()};

    // extractSubset will not acutually make a copy, and Skia will refer to the
    // original data which is not what we want, since rows won't be contiguous.
    // However, since this is very cheap, we can do it and *then* make a copy.
    SkBitmap non_copied_subset;
    transport_bitmap.extractSubset(&non_copied_subset, sk_copy_rect);
    non_copied_subset.copyTo(&copy_bitmap, SkBitmap::kARGB_8888_Config);
    CHECK(!copy_bitmap.isNull());
  }

  glBindTexture(GL_TEXTURE_2D, texture_id_);

  SkAutoLockPixels lock(copy_bitmap);
  if (copy_rect.size() == size_ && copy_rect.size() != texture_size_) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, copy_rect.width(),
                 copy_rect.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE,
                 copy_bitmap.getAddr32(0, 0));
    texture_size_ = copy_rect.size();
  } else {
    /* Debugging code for why the below call may fail.
    int existing_width = 0, existing_height = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,
                             &existing_width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,
                             &existing_height);
    */
    glTexSubImage2D(GL_TEXTURE_2D, 0, copy_rect.x(), copy_rect.y(),
                    copy_rect.width(), copy_rect.height(),
                    GL_BGRA, GL_UNSIGNED_BYTE,
                    copy_bitmap.getAddr32(0, 0));
    /* Enable if you're having problems with TexSubImage failing.
    int err = glGetError();
    DCHECK(err == GL_NO_ERROR) << "Error " << err <<
           " copying (" << copy_rect.x() << "," << copy_rect.y() <<
           ")," << copy_rect.width() << "x" << copy_rect.height() <<
           " for bitmap " << texture_size_.width() << "x" <<
           texture_size_.height() <<
           " real size " << existing_width << "x" << existing_height <<
           " for " << this;
    */
  }
}
