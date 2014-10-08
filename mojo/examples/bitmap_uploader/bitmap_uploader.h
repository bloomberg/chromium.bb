// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_BITMAP_UPLOADER_BITMAP_UPLOADER_H_
#define MOJO_EXAMPLES_BITMAP_UPLOADER_BITMAP_UPLOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class SurfaceIdAllocator;
}

namespace mojo {
class Shell;
class View;

// BitmapUploader is useful if you want to draw a bitmap or color in a View.
class BitmapUploader : public SurfaceClient {
 public:
  explicit BitmapUploader(View* view);
  virtual ~BitmapUploader();

  void Init(Shell* shell);

  void SetColor(SkColor color);
  void SetBitmap(const SkBitmap& bitmap);

 private:
  void Upload();
  void OnSurfaceConnectionCreated(SurfacePtr surface, uint32_t id_namespace);
  uint32_t BindTextureForSize(const gfx::Size size);

  // SurfaceClient implementation.
  virtual void ReturnResources(Array<ReturnedResourcePtr> resources) override;

  View* view_;
  SurfacesServicePtr surfaces_service_;
  GpuPtr gpu_service_;
  MojoGLES2Context gles2_context_;

  gfx::Size size_;
  SkColor color_;
  SkBitmap bitmap_;
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

#endif  // MOJO_EXAMPLES_BITMAP_UPLOADER_BITMAP_UPLOADER_H_
