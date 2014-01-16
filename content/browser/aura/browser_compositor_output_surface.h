// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AURA_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_AURA_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/id_map.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/output_surface.h"
#include "content/common/content_export.h"

namespace base { class MessageLoopProxy; }

namespace cc {
class SoftwareOutputDevice;
}

namespace ui { class Compositor; }

namespace content {
class ContextProviderCommandBuffer;
class ReflectorImpl;
class WebGraphicsContext3DCommandBufferImpl;

class CONTENT_EXPORT BrowserCompositorOutputSurface
    : public cc::OutputSurface,
      public base::NonThreadSafe {
 public:
  virtual ~BrowserCompositorOutputSurface();

  // cc::OutputSurface implementation.
  virtual bool BindToClient(cc::OutputSurfaceClient* client) OVERRIDE;
  virtual void Reshape(gfx::Size size, float scale_factor) OVERRIDE;

  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval);

  void SetReflector(ReflectorImpl* reflector);

 protected:
  // Constructor used by the accelerated implementation.
  BrowserCompositorOutputSurface(
      const scoped_refptr<ContextProviderCommandBuffer>& context,
      int surface_id,
      IDMap<BrowserCompositorOutputSurface>* output_surface_map,
      base::MessageLoopProxy* compositor_message_loop,
      base::WeakPtr<ui::Compositor> compositor);

  // Constructor used by the software implementation.
  BrowserCompositorOutputSurface(
      scoped_ptr<cc::SoftwareOutputDevice> software_device,
      int surface_id,
      IDMap<BrowserCompositorOutputSurface>* output_surface_map,
      base::MessageLoopProxy* compositor_message_loop,
      base::WeakPtr<ui::Compositor> compositor);

  int surface_id_;
  IDMap<BrowserCompositorOutputSurface>* output_surface_map_;

  scoped_refptr<base::MessageLoopProxy> compositor_message_loop_;
  base::WeakPtr<ui::Compositor> compositor_;
  scoped_refptr<ReflectorImpl> reflector_;

 private:
  void Initialize();

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_AURA_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
