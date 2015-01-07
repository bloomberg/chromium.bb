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
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/android/media_player_android.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"

namespace content {
namespace {

// Pass a java surface object to the MediaPlayerAndroid object
// identified by render process handle, render frame ID and player ID.
static void SetSurfacePeer(scoped_refptr<gfx::SurfaceTexture> surface_texture,
                           base::ProcessHandle render_process_handle,
                           int render_frame_id,
                           int player_id) {
  int render_process_id = 0;
  RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
  while (!it.IsAtEnd()) {
    if (it.GetCurrentValue()->GetHandle() == render_process_handle) {
      render_process_id = it.GetCurrentValue()->GetID();
      break;
    }
    it.Advance();
  }
  if (!render_process_id) {
    DVLOG(1) << "Cannot find render process for render_process_handle "
             << render_process_handle;
    return;
  }

  RenderFrameHostImpl* frame =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!frame) {
    DVLOG(1) << "Cannot find frame for render_frame_id " << render_frame_id;
    return;
  }

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(frame));
  BrowserMediaPlayerManager* player_manager =
      web_contents->media_web_contents_observer()->GetMediaPlayerManager(frame);
  if (!player_manager) {
    DVLOG(1) << "Cannot find the media player manager for frame " << frame;
    return;
  }

  media::MediaPlayerAndroid* player = player_manager->GetPlayer(player_id);
  if (!player) {
    DVLOG(1) << "Cannot find media player for player_id " << player_id;
    return;
  }

  if (player != player_manager->GetFullscreenPlayer()) {
    gfx::ScopedJavaSurface scoped_surface(surface_texture.get());
    player->SetVideoSurface(scoped_surface.Pass());
  }
}

}  // namespace

BrowserSurfaceTextureManager::BrowserSurfaceTextureManager() {
  SurfaceTexturePeer::InitInstance(this);
}

BrowserSurfaceTextureManager::~BrowserSurfaceTextureManager() {
  SurfaceTexturePeer::InitInstance(NULL);
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

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&SetSurfacePeer,
                                     surface_texture,
                                     render_process_handle,
                                     render_frame_id,
                                     player_id));
}

}  // namespace content
