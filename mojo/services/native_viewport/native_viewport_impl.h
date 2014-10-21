// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_IMPL_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_IMPL_H_

#include "base/macros.h"
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
  ~NativeViewportImpl() override;

  // InterfaceImpl<NativeViewport> implementation.
  void Create(SizePtr size, const Callback<void(uint64_t)>& callback) override;
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetSize(SizePtr size) override;
  void SubmittedFrame(SurfaceIdPtr surface_id) override;

  // PlatformViewport::Delegate implementation.
  void OnBoundsChanged(const gfx::Rect& bounds) override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  bool OnEvent(ui::Event* ui_event) override;
  void OnDestroyed() override;

  void AckEvent();

 private:
  void ProcessOnBoundsChanged();

  bool is_headless_;
  scoped_ptr<PlatformViewport> platform_viewport_;
  scoped_ptr<ViewportSurface> viewport_surface_;
  uint64_t widget_id_;
  gfx::Size size_;
  GpuPtr gpu_service_;
  SurfacesServicePtr surfaces_service_;
  cc::SurfaceId child_surface_id_;
  bool waiting_for_event_ack_;
  Callback<void(uint64_t)> create_callback_;
  base::WeakPtrFactory<NativeViewportImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_IMPL_H_
