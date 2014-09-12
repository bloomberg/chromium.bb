// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_VIEWPORT_SURFACE_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_VIEWPORT_SURFACE_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace cc {
class SurfaceIdAllocator;
}

namespace mojo {

// This manages the surface that draws to a particular NativeViewport instance.
class ViewportSurface : public SurfaceClient {
 public:
  ViewportSurface(SurfacesService* surfaces_service,
                  Gpu* gpu_service,
                  const gfx::Size& size,
                  cc::SurfaceId child_id);
  virtual ~ViewportSurface();

  void SetWidgetId(uint64_t widget_id);
  void SetSize(const gfx::Size& size);
  void SetChildId(cc::SurfaceId child_id);

 private:
  void OnSurfaceConnectionCreated(SurfacePtr surface, uint32_t id_namespace);
  void BindSurfaceToNativeViewport();
  void SubmitFrame();

  // SurfaceClient implementation.
  virtual void ReturnResources(Array<ReturnedResourcePtr> resources) OVERRIDE;

  SurfacePtr surface_;
  Gpu* gpu_service_;
  uint64_t widget_id_;
  gfx::Size size_;
  scoped_ptr<cc::SurfaceIdAllocator> id_allocator_;
  cc::SurfaceId id_;
  cc::SurfaceId child_id_;
  base::WeakPtrFactory<ViewportSurface> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViewportSurface);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_VIEWPORT_SURFACE_H_
