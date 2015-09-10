// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process_surface_texture_manager.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "base/android/jni_android.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/logging.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
InProcessSurfaceTextureManager* InProcessSurfaceTextureManager::GetInstance() {
  return base::Singleton<
      InProcessSurfaceTextureManager,
      base::LeakySingletonTraits<InProcessSurfaceTextureManager>>::get();
}

void InProcessSurfaceTextureManager::RegisterSurfaceTexture(
    int surface_texture_id,
    int client_id,
    gfx::SurfaceTexture* surface_texture) {
  base::AutoLock lock(lock_);

  DCHECK(surface_textures_.find(surface_texture_id) == surface_textures_.end());
  surface_textures_.add(
      surface_texture_id,
      make_scoped_ptr(new gfx::ScopedJavaSurface(surface_texture)));
}

void InProcessSurfaceTextureManager::UnregisterSurfaceTexture(
    int surface_texture_id,
    int client_id) {
  base::AutoLock lock(lock_);

  DCHECK(surface_textures_.find(surface_texture_id) != surface_textures_.end());
  surface_textures_.erase(surface_texture_id);
}

gfx::AcceleratedWidget
InProcessSurfaceTextureManager::AcquireNativeWidgetForSurfaceTexture(
    int surface_texture_id) {
  base::AutoLock lock(lock_);

  DCHECK(surface_textures_.find(surface_texture_id) != surface_textures_.end());
  JNIEnv* env = base::android::AttachCurrentThread();
  return ANativeWindow_fromSurface(
      env, surface_textures_.get(surface_texture_id)->j_surface().obj());
}

void InProcessSurfaceTextureManager::EstablishSurfaceTexturePeer(
    base::ProcessHandle render_process_handle,
    scoped_refptr<gfx::SurfaceTexture> surface_texture,
    int render_frame_id,
    int player_id) {
  if (!surface_texture.get())
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowserMediaPlayerManager::SetSurfacePeer, surface_texture,
                 render_process_handle, render_frame_id, player_id));
}

InProcessSurfaceTextureManager::InProcessSurfaceTextureManager() {
  SurfaceTexturePeer::InitInstance(this);
}

InProcessSurfaceTextureManager::~InProcessSurfaceTextureManager() {
  SurfaceTexturePeer::InitInstance(nullptr);
}

}  // namespace content
