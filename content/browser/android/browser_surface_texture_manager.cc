// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_surface_texture_manager.h"

#include <android/native_window_jni.h>

#include "base/android/jni_android.h"
#include "content/browser/android/child_process_launcher_android.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"

namespace content {

// static
BrowserSurfaceTextureManager* BrowserSurfaceTextureManager::GetInstance() {
  return base::Singleton<
      BrowserSurfaceTextureManager,
      base::LeakySingletonTraits<BrowserSurfaceTextureManager>>::get();
}

void BrowserSurfaceTextureManager::RegisterSurfaceTexture(
    int surface_texture_id,
    int client_id,
    gfx::SurfaceTexture* surface_texture) {
  content::CreateSurfaceTextureSurface(
      surface_texture_id, client_id, surface_texture);
}

void BrowserSurfaceTextureManager::UnregisterSurfaceTexture(
    int surface_texture_id,
    int client_id) {
  content::DestroySurfaceTextureSurface(surface_texture_id, client_id);
}

gfx::AcceleratedWidget
BrowserSurfaceTextureManager::AcquireNativeWidgetForSurfaceTexture(
    int surface_texture_id) {
  gfx::ScopedJavaSurface surface(
      content::GetSurfaceTextureSurface(
          surface_texture_id,
          BrowserGpuChannelHostFactory::instance()->GetGpuChannelId()));
  if (surface.j_surface().is_null())
    return NULL;

  JNIEnv* env = base::android::AttachCurrentThread();
  // Note: This ensures that any local references used by
  // ANativeWindow_fromSurface are released immediately. This is needed as a
  // workaround for https://code.google.com/p/android/issues/detail?id=68174
  base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
  ANativeWindow* native_window =
      ANativeWindow_fromSurface(env, surface.j_surface().obj());

  return native_window;
}

void BrowserSurfaceTextureManager::EstablishSurfaceTexturePeer(
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

BrowserSurfaceTextureManager::BrowserSurfaceTextureManager() {
  SurfaceTexturePeer::InitInstance(this);
}

BrowserSurfaceTextureManager::~BrowserSurfaceTextureManager() {
  SurfaceTexturePeer::InitInstance(nullptr);
}

}  // namespace content
