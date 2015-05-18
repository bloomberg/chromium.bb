// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_MANAGER_H_
#define CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_MANAGER_H_

#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class SurfaceTexture;
}

namespace content {

class CONTENT_EXPORT SurfaceTextureManager {
 public:
  static SurfaceTextureManager* GetInstance();
  static void SetInstance(SurfaceTextureManager* instance);

  // Register a surface texture for use in another process.
  virtual void RegisterSurfaceTexture(int surface_texture_id,
                                      int client_id,
                                      gfx::SurfaceTexture* surface_texture) = 0;

  // Unregister a surface texture previously registered for use in another
  // process.
  virtual void UnregisterSurfaceTexture(int surface_texture_id,
                                        int client_id) = 0;

  // Acquire native widget for a registered surface texture.
  virtual gfx::AcceleratedWidget AcquireNativeWidgetForSurfaceTexture(
      int surface_texture_id) = 0;

 protected:
  virtual ~SurfaceTextureManager() {}
};

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_MANAGER_H_
