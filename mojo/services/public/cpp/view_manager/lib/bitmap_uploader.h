// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_BITMAP_UPLOADER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_BITMAP_UPLOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class SurfaceIdAllocator;
}

namespace mojo {
class ViewManagerClientImpl;

class BitmapUploader : public SurfaceClient {
 public:
  BitmapUploader(ViewManagerClientImpl* client,
                 Id view_id,
                 SurfacesServicePtr surfaces_service,
                 GpuPtr gpu_service);
  virtual ~BitmapUploader();

  void SetSize(const gfx::Size& size);
  void SetColor(SkColor color);
  void SetBitmap(SkBitmap bitmap);
  void SetDoneCallback(const base::Callback<void(SurfaceIdPtr)>& done_callback);

 private:
  void Upload();
  void OnSurfaceConnectionCreated(SurfacePtr surface, uint32_t id_namespace);
  uint32_t BindTextureForSize(const gfx::Size size);

  // SurfaceClient implementation.
  virtual void ReturnResources(Array<ReturnedResourcePtr> resources) OVERRIDE;

  ViewManagerClientImpl* client_;
  Id view_id_;
  SurfacesServicePtr surfaces_service_;
  GpuPtr gpu_service_;
  MojoGLES2Context gles2_context_;

  gfx::Size size_;
  SkColor color_;
  SkBitmap bitmap_;
  base::Callback<void(SurfaceIdPtr)> done_callback_;
  SurfacePtr surface_;
  cc::SurfaceId id_;
  scoped_ptr<cc::SurfaceIdAllocator> id_allocator_;
  gfx::Size surface_size_;
  uint32_t next_resource_id_;
  base::hash_map<uint32_t, uint32_t> resource_to_texture_id_map_;

  base::WeakPtrFactory<BitmapUploader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BitmapUploader);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_BITMAP_UPLOADER_H_
