// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_mac.h"

#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/platform_thread.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/common/content_constants_internal.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gl/gl_context.h"

#ifdef NDEBUG
#define CHECK_GL_ERROR()
#define CHECK_AND_SAVE_GL_ERROR()
#else
#define CHECK_GL_ERROR() do {                                           \
    GLenum gl_error = glGetError();                                     \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error: " << gl_error; \
  } while (0)
#define CHECK_AND_SAVE_GL_ERROR() do {                                  \
    GLenum gl_error = GetAndSaveGLError();                              \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error: " << gl_error; \
  } while (0)
#endif

namespace content {

// static
scoped_refptr<CompositingIOSurfaceMac> CompositingIOSurfaceMac::Create() {
  scoped_refptr<CompositingIOSurfaceContext> offscreen_context =
      CompositingIOSurfaceContext::Get(
          CompositingIOSurfaceContext::kOffscreenContextWindowNumber);
  if (!offscreen_context.get()) {
    LOG(ERROR) << "Failed to create context for offscreen operations";
    return NULL;
  }

  return new CompositingIOSurfaceMac(offscreen_context);
}

CompositingIOSurfaceMac::CompositingIOSurfaceMac(
    const scoped_refptr<CompositingIOSurfaceContext>& offscreen_context)
    : offscreen_context_(offscreen_context),
      io_surface_handle_(0),
      scale_factor_(1.f),
      texture_(0),
      gl_error_(GL_NO_ERROR),
      eviction_queue_iterator_(eviction_queue_.Get().end()),
      eviction_has_been_drawn_since_updated_(false) {
  CHECK(offscreen_context_.get());
}

CompositingIOSurfaceMac::~CompositingIOSurfaceMac() {
  {
    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(
        offscreen_context_->cgl_context());
    UnrefIOSurfaceWithContextCurrent();
  }
  offscreen_context_ = NULL;
  DCHECK(eviction_queue_iterator_ == eviction_queue_.Get().end());
}

bool CompositingIOSurfaceMac::SetIOSurfaceWithContextCurrent(
    scoped_refptr<CompositingIOSurfaceContext> current_context,
    IOSurfaceID io_surface_handle,
    const gfx::Size& size,
    float scale_factor) {
  bool result = MapIOSurfaceToTextureWithContextCurrent(
      current_context, size, scale_factor, io_surface_handle);
  EvictionMarkUpdated();
  return result;
}

int CompositingIOSurfaceMac::GetRendererID() {
  GLint current_renderer_id = -1;
  if (CGLGetParameter(offscreen_context_->cgl_context(),
                      kCGLCPCurrentRendererID,
                      &current_renderer_id) == kCGLNoError)
    return current_renderer_id & kCGLRendererIDMatchingMask;
  return -1;
}

bool CompositingIOSurfaceMac::DrawIOSurface(
    scoped_refptr<CompositingIOSurfaceContext> drawing_context,
    const gfx::Rect& window_rect,
    float window_scale_factor) {
  DCHECK_EQ(CGLGetCurrentContext(), drawing_context->cgl_context());

  bool has_io_surface = HasIOSurface();
  TRACE_EVENT1("browser", "CompositingIOSurfaceMac::DrawIOSurface",
               "has_io_surface", has_io_surface);

  gfx::Rect pixel_window_rect =
      ToNearestRect(gfx::ScaleRect(window_rect, window_scale_factor));
  glViewport(
      pixel_window_rect.x(), pixel_window_rect.y(),
      pixel_window_rect.width(), pixel_window_rect.height());

  SurfaceQuad quad;
  quad.set_size(dip_io_surface_size_, pixel_io_surface_size_);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  // Note that the projection keeps things in view units, so the use of
  // window_rect / dip_io_surface_size_ (as opposed to the pixel_ variants)
  // below is correct.
  glOrtho(0, window_rect.width(), window_rect.height(), 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  glColor4f(1, 1, 1, 1);
  if (has_io_surface) {
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_);
    DrawQuad(quad);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    glDisable(GL_TEXTURE_RECTANGLE_ARB);
    CHECK_AND_SAVE_GL_ERROR();

    // Fill the resize gutters with white.
    if (window_rect.width() > dip_io_surface_size_.width() ||
        window_rect.height() > dip_io_surface_size_.height()) {
      SurfaceQuad filler_quad;
      if (window_rect.width() > dip_io_surface_size_.width()) {
        // Draw right-side gutter down to the bottom of the window.
        filler_quad.set_rect(dip_io_surface_size_.width(), 0.0f,
                             window_rect.width(), window_rect.height());
        DrawQuad(filler_quad);
      }
      if (window_rect.height() > dip_io_surface_size_.height()) {
        // Draw bottom gutter to the width of the IOSurface.
        filler_quad.set_rect(
            0.0f, dip_io_surface_size_.height(),
            dip_io_surface_size_.width(), window_rect.height());
        DrawQuad(filler_quad);
      }
    }

    // Workaround for issue 158469. Issue a dummy draw call with texture_ not
    // bound to a texture, in order to shake all references to the IOSurface out
    // of the driver.
    glBegin(GL_TRIANGLES);
    glEnd();
    CHECK_AND_SAVE_GL_ERROR();
  } else {
    // Should match the clear color of RenderWidgetHostViewMac.
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  bool workaround_needed =
      GpuDataManagerImpl::GetInstance()->IsDriverBugWorkaroundActive(
          gpu::FORCE_GL_FINISH_AFTER_COMPOSITING);
  if (workaround_needed) {
    TRACE_EVENT0("gpu", "glFinish");
    glFinish();
  }

  // Check if any of the drawing calls result in an error.
  GetAndSaveGLError();
  bool result = true;
  if (gl_error_ != GL_NO_ERROR) {
    LOG(ERROR) << "GL error in DrawIOSurface: " << gl_error_;
    result = false;
    // If there was an error, clear the screen to a light grey to avoid
    // rendering artifacts. If we're in a really bad way, this too may
    // generate an error. Clear the GL error afterwards just in case.
    glClearColor(0.8, 0.8, 0.8, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glGetError();
  }

  eviction_has_been_drawn_since_updated_ = true;
  return result;
}

bool CompositingIOSurfaceMac::MapIOSurfaceToTextureWithContextCurrent(
    const scoped_refptr<CompositingIOSurfaceContext>& current_context,
    const gfx::Size pixel_size,
    float scale_factor,
    IOSurfaceID io_surface_handle) {
  TRACE_EVENT0("browser", "CompositingIOSurfaceMac::MapIOSurfaceToTexture");

  if (!io_surface_ || io_surface_handle != io_surface_handle_)
    UnrefIOSurfaceWithContextCurrent();

  pixel_io_surface_size_ = pixel_size;
  scale_factor_ = scale_factor;
  dip_io_surface_size_ = gfx::ToFlooredSize(
      gfx::ScaleSize(pixel_io_surface_size_, 1.0 / scale_factor_));

  // Early-out if the IOSurface has not changed. Note that because IOSurface
  // sizes are rounded, the same IOSurface may have two different sizes
  // associated with it.
  if (io_surface_ && io_surface_handle == io_surface_handle_)
    return true;

  io_surface_.reset(IOSurfaceLookup(io_surface_handle));
  // Can fail if IOSurface with that ID was already released by the gpu
  // process.
  if (!io_surface_) {
    UnrefIOSurfaceWithContextCurrent();
    return false;
  }

  io_surface_handle_ = io_surface_handle;

  // Actual IOSurface size is rounded up to reduce reallocations during window
  // resize. Get the actual size to properly map the texture.
  gfx::Size rounded_size(IOSurfaceGetWidth(io_surface_),
                         IOSurfaceGetHeight(io_surface_));

  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_);
  glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  CHECK_AND_SAVE_GL_ERROR();
  GLuint plane = 0;
  CGLError cgl_error = CGLTexImageIOSurface2D(
      current_context->cgl_context(),
      GL_TEXTURE_RECTANGLE_ARB,
      GL_RGBA,
      rounded_size.width(),
      rounded_size.height(),
      GL_BGRA,
      GL_UNSIGNED_INT_8_8_8_8_REV,
      io_surface_.get(),
      plane);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
  if (cgl_error != kCGLNoError) {
    LOG(ERROR) << "CGLTexImageIOSurface2D: " << cgl_error;
    UnrefIOSurfaceWithContextCurrent();
    return false;
  }
  GetAndSaveGLError();
  if (gl_error_ != GL_NO_ERROR) {
    LOG(ERROR) << "GL error in MapIOSurfaceToTexture: " << gl_error_;
    UnrefIOSurfaceWithContextCurrent();
    return false;
  }
  return true;
}

void CompositingIOSurfaceMac::UnrefIOSurface() {
  gfx::ScopedCGLSetCurrentContext scoped_set_current_context(
      offscreen_context_->cgl_context());
  UnrefIOSurfaceWithContextCurrent();
}

void CompositingIOSurfaceMac::DrawQuad(const SurfaceQuad& quad) {
  TRACE_EVENT0("gpu", "CompositingIOSurfaceMac::DrawQuad");

  glEnableClientState(GL_VERTEX_ARRAY); CHECK_AND_SAVE_GL_ERROR();
  glEnableClientState(GL_TEXTURE_COORD_ARRAY); CHECK_AND_SAVE_GL_ERROR();

  glVertexPointer(2, GL_FLOAT, sizeof(SurfaceVertex), &quad.verts_[0].x_);
  glTexCoordPointer(2, GL_FLOAT, sizeof(SurfaceVertex), &quad.verts_[0].tx_);
  glDrawArrays(GL_QUADS, 0, 4); CHECK_AND_SAVE_GL_ERROR();

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void CompositingIOSurfaceMac::UnrefIOSurfaceWithContextCurrent() {
  if (texture_) {
    glDeleteTextures(1, &texture_);
    texture_ = 0;
  }
  pixel_io_surface_size_ = gfx::Size();
  scale_factor_ = 1;
  dip_io_surface_size_ = gfx::Size();
  io_surface_.reset();

  // Forget the ID, because even if it is still around when we want to use it
  // again, OSX may have reused the same ID for a new tab and we don't want to
  // blit random tab contents.
  io_surface_handle_ = 0;

  EvictionMarkEvicted();
}

bool CompositingIOSurfaceMac::HasBeenPoisoned() const {
  return offscreen_context_->HasBeenPoisoned();
}

GLenum CompositingIOSurfaceMac::GetAndSaveGLError() {
  GLenum gl_error = glGetError();
  if (gl_error_ == GL_NO_ERROR)
    gl_error_ = gl_error;
  return gl_error;
}

void CompositingIOSurfaceMac::EvictionMarkUpdated() {
  EvictionMarkEvicted();
  eviction_queue_.Get().push_back(this);
  eviction_queue_iterator_ = --eviction_queue_.Get().end();
  eviction_has_been_drawn_since_updated_ = false;
  EvictionScheduleDoEvict();
}

void CompositingIOSurfaceMac::EvictionMarkEvicted() {
  if (eviction_queue_iterator_ == eviction_queue_.Get().end())
    return;
  eviction_queue_.Get().erase(eviction_queue_iterator_);
  eviction_queue_iterator_ = eviction_queue_.Get().end();
  eviction_has_been_drawn_since_updated_ = false;
}

// static
void CompositingIOSurfaceMac::EvictionScheduleDoEvict() {
  if (eviction_scheduled_)
    return;
  if (eviction_queue_.Get().size() <= kMaximumUnevictedSurfaces)
    return;

  eviction_scheduled_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CompositingIOSurfaceMac::EvictionDoEvict));
}

// static
void CompositingIOSurfaceMac::EvictionDoEvict() {
  eviction_scheduled_ = false;
  // Walk the list of allocated surfaces from least recently used to most
  // recently used.
  for (EvictionQueue::iterator it = eviction_queue_.Get().begin();
       it != eviction_queue_.Get().end();) {
    CompositingIOSurfaceMac* surface = *it;
    ++it;

    // If the number of IOSurfaces allocated is less than the threshold,
    // stop walking the list of surfaces.
    if (eviction_queue_.Get().size() <= kMaximumUnevictedSurfaces)
      break;

    // Don't evict anything that has not yet been drawn.
    if (!surface->eviction_has_been_drawn_since_updated_)
      continue;

    // Evict the surface.
    surface->UnrefIOSurface();
  }
}

// static
base::LazyInstance<CompositingIOSurfaceMac::EvictionQueue>
    CompositingIOSurfaceMac::eviction_queue_;

// static
bool CompositingIOSurfaceMac::eviction_scheduled_ = false;

}  // namespace content
