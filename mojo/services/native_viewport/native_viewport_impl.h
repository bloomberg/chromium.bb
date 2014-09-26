// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_IMPL_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/services/native_viewport/platform_viewport.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class Event;
}

namespace mojo {
class ApplicationImpl;
class ViewportSurface;

class NativeViewportImpl : public InterfaceImpl<NativeViewport>,
                           public PlatformViewport::Delegate {
 public:
  NativeViewportImpl(ApplicationImpl* app, bool is_headless);
  virtual ~NativeViewportImpl();

  // InterfaceImpl<NativeViewport> implementation.
  virtual void Create(SizePtr bounds) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void SetBounds(SizePtr bounds) OVERRIDE;
  virtual void SubmittedFrame(SurfaceIdPtr surface_id) OVERRIDE;

  // PlatformViewport::Delegate implementation.
  virtual void OnBoundsChanged(const gfx::Rect& bounds) OVERRIDE;
  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE;
  virtual bool OnEvent(ui::Event* ui_event) OVERRIDE;
  virtual void OnDestroyed() OVERRIDE;

  void AckEvent();

 private:
  bool is_headless_;
  scoped_ptr<PlatformViewport> platform_viewport_;
  scoped_ptr<ViewportSurface> viewport_surface_;
  uint64_t widget_id_;
  gfx::Rect bounds_;
  GpuPtr gpu_service_;
  SurfacesServicePtr surfaces_service_;
  cc::SurfaceId child_surface_id_;
  bool waiting_for_event_ack_;
  base::WeakPtrFactory<NativeViewportImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_IMPL_H_
