// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_BROWSER_SURFACE_TEXTURE_MANAGER_H_
#define CONTENT_BROWSER_ANDROID_BROWSER_SURFACE_TEXTURE_MANAGER_H_

#include "content/common/android/surface_texture_manager.h"

#include "content/common/android/surface_texture_peer.h"

namespace content {

class BrowserSurfaceTextureManager : public SurfaceTextureManager,
                                     public SurfaceTexturePeer {
 public:
  BrowserSurfaceTextureManager();
  virtual ~BrowserSurfaceTextureManager();

  // Overridden from SurfaceTextureManager:
  virtual void RegisterSurfaceTexture(
      int surface_texture_id,
      int client_id,
      gfx::SurfaceTexture* surface_texture) override;
  virtual void UnregisterSurfaceTexture(int surface_texture_id,
                                        int client_id) override;
  virtual gfx::AcceleratedWidget AcquireNativeWidgetForSurfaceTexture(
      int surface_texture_id) override;

  // Overridden from SurfaceTexturePeer:
  virtual void EstablishSurfaceTexturePeer(
      base::ProcessHandle render_process_handle,
      scoped_refptr<gfx::SurfaceTexture> surface_texture,
      int render_frame_id,
      int player_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserSurfaceTextureManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_BROWSER_SURFACE_TEXTURE_MANAGER_H_
