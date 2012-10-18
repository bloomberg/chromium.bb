// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_IMAGE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_IMAGE_MANAGER_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "gpu/gpu_export.h"

namespace gfx {
class GLImage;
}

namespace gpu {
namespace gles2 {

// Interface used by the cmd decoder to lookup images.
class GPU_EXPORT ImageManager : public base::RefCounted<ImageManager> {
 public:
  ImageManager();

  void AddImage(gfx::GLImage* gl_image, int32 service_id);
  void RemoveImage(int32 service_id);
  gfx::GLImage* LookupImage(int32 service_id);

 private:
  friend class base::RefCounted<ImageManager>;

  ~ImageManager();

  typedef base::hash_map<uint32, scoped_refptr<gfx::GLImage> > GLImageMap;
  GLImageMap gl_images_;

  DISALLOW_COPY_AND_ASSIGN(ImageManager);
};

}  // namespage gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_IMAGE_MANAGER_H_
