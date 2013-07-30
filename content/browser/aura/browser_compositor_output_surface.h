// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AURA_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_AURA_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/output_surface.h"

namespace base { class MessageLoopProxy; }

namespace ui { class Compositor; }

namespace content {
class ReflectorImpl;

// Adapts a WebGraphicsContext3DCommandBufferImpl into a
// cc::OutputSurface that also handles vsync parameter updates
// arriving from the GPU process.
class BrowserCompositorOutputSurface
    : public cc::OutputSurface,
      public base::NonThreadSafe {
 public:
  BrowserCompositorOutputSurface(
      scoped_ptr<WebKit::WebGraphicsContext3D> context,
      int surface_id,
      IDMap<BrowserCompositorOutputSurface>* output_surface_map,
      base::MessageLoopProxy* compositor_message_loop,
      base::WeakPtr<ui::Compositor> compositor);

  virtual ~BrowserCompositorOutputSurface();

  // cc::OutputSurface implementation.
  virtual bool BindToClient(cc::OutputSurfaceClient* client) OVERRIDE;
  virtual void Reshape(gfx::Size size, float scale_factor) OVERRIDE;
  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE;

  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval);

  void SetReflector(ReflectorImpl* reflector);

 private:
  int surface_id_;
  IDMap<BrowserCompositorOutputSurface>* output_surface_map_;

  scoped_refptr<base::MessageLoopProxy> compositor_message_loop_;
  base::WeakPtr<ui::Compositor> compositor_;
  scoped_refptr<ReflectorImpl> reflector_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AURA_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
