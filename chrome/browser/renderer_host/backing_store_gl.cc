// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/backing_store.h"

#include <GL/gl.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/renderer_host/backing_store_manager.h"
#include "chrome/browser/renderer_host/backing_store_manager_glx.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/transport_dib.h"
#include "chrome/common/x11_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"

BackingStore::BackingStore(RenderWidgetHost* widget,
                           const gfx::Size& size,
                           void* visual,
                           int depth)
    : render_widget_host_(widget),
      size_(size),
      display_(x11_util::GetXDisplay()),
      root_window_(x11_util::GetX11RootWindow()),
      texture_id_(0) {
  XID id = x11_util::GetX11WindowFromGtkWidget(widget->view()->GetNativeView());
  BackingStoreManager::GetGlManager()->BindContext(id);

  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);

  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  DCHECK(glGetError() == GL_NO_ERROR);
}

BackingStore::BackingStore(RenderWidgetHost* widget, const gfx::Size& size)
    : render_widget_host_(widget),
      size_(size),
      display_(NULL),
      root_window_(0),
      texture_id_(0) {
}

BackingStore::~BackingStore() {
  if (texture_id_)
    glDeleteTextures(1, &texture_id_);
}

void BackingStore::ShowRect(const gfx::Rect& damage, XID target) {
  DCHECK(texture_id_ > 0);

  // TODO(brettw) is this necessray?
  XID id = x11_util::GetX11WindowFromGtkWidget(
      render_widget_host_->view()->GetNativeView());
  BackingStoreManager::GetGlManager()->BindContext(id);

  glViewport(0, 0, size_.width(), size_.height());

  // TODO(brettw) only repaint the damaged area. This currently erases and
  // repaints the entire screen.

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture_id_);

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

  glXSwapBuffers(display_, id);
}

SkBitmap BackingStore::PaintRectToBitmap(const gfx::Rect& rect) {
  NOTIMPLEMENTED();
  return SkBitmap();
}

#if defined(TOOLKIT_GTK)
void BackingStore::PaintToRect(const gfx::Rect& rect, GdkDrawable* target) {
  NOTIMPLEMENTED();
}
#endif

// Paint the given transport DIB into our backing store.
void BackingStore::PaintRect(base::ProcessHandle process,
                             TransportDIB* bitmap,
                             const gfx::Rect& bitmap_rect,
                             const gfx::Rect& copy_rect) {
  if (!display_)
    return;

  if (bitmap_rect.IsEmpty() || copy_rect.IsEmpty())
    return;

  scoped_ptr<skia::PlatformCanvas> canvas(
      bitmap->GetPlatformCanvas(bitmap_rect.width(), bitmap_rect.height()));
  const SkBitmap& transport_bitmap =
      canvas->getTopPlatformDevice().accessBitmap(false);

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
    SkIRect subset;
    subset.fLeft = copy_rect.x() - bitmap_rect.x();
    subset.fTop = copy_rect.y() - bitmap_rect.y();
    subset.fRight = subset.fLeft + copy_rect.width();
    subset.fBottom = subset.fTop + copy_rect.height();
    SkIRect sk_copy_rect = { copy_rect.x() - bitmap_rect.x(),
                             copy_rect.y() - bitmap_rect.y(),
                             copy_rect.right(), copy_rect.bottom() };

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
    DCHECK(glGetError() == GL_NO_ERROR);
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
    DCHECK(glGetError() == GL_NO_ERROR);
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

void BackingStore::ScrollRect(base::ProcessHandle process,
                              TransportDIB* bitmap,
                              const gfx::Rect& bitmap_rect,
                              int dx, int dy,
                              const gfx::Rect& clip_rect,
                              const gfx::Size& view_size) {
  NOTIMPLEMENTED();
}

size_t BackingStore::MemorySize() {
  return texture_size_.GetArea() * 4;
}
