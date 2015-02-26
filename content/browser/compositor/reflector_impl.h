// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_REFLECTOR_IMPL_H_
#define CONTENT_BROWSER_COMPOSITOR_REFLECTOR_IMPL_H_

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ui/compositor/reflector.h"
#include "ui/gfx/geometry/size.h"

namespace base { class MessageLoopProxy; }

namespace gfx { class Rect; }

namespace ui {
class Compositor;
class Layer;
}

namespace content {

class OwnedMailbox;
class BrowserCompositorOutputSurface;

// A reflector implementation that copies the framebuffer content
// to the texture, then draw it onto the mirroring compositor.
class CONTENT_EXPORT ReflectorImpl
    : public base::SupportsWeakPtr<ReflectorImpl>,
      public ui::Reflector {
 public:
  ReflectorImpl(ui::Compositor* mirrored_compositor,
                ui::Layer* mirroring_layer);
  ~ReflectorImpl() override;

  ui::Compositor* mirrored_compositor() { return mirrored_compositor_; }

  void Shutdown();

  void DetachFromOutputSurface();

  // ui::Reflector implementation.
  void OnMirroringCompositorResized() override;

  // Called in |BrowserCompositorOutputSurface::SwapBuffers| to copy
  // the full screen image to the |mailbox_| texture.
  void OnSourceSwapBuffers();

  // Called in |BrowserCompositorOutputSurface::PostSubBuffer| copy
  // the sub image given by |rect| to the |mailbox_| texture.
  void OnSourcePostSubBuffer(const gfx::Rect& rect);

  // Called when the source surface is bound and available.
  void OnSourceSurfaceReady(BrowserCompositorOutputSurface* surface);

 private:
  void UpdateTexture(const gfx::Size& size, const gfx::Rect& redraw_rect);

  ui::Compositor* mirrored_compositor_;
  ui::Layer* mirroring_layer_;
  scoped_refptr<OwnedMailbox> mailbox_;
  scoped_ptr<GLHelper> mirrored_compositor_gl_helper_;
  int mirrored_compositor_gl_helper_texture_id_;
  bool needs_set_mailbox_;
  bool flip_texture_;
  BrowserCompositorOutputSurface* output_surface_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_REFLECTOR_IMPL_H_
