// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/surface_texture_peer_browser_impl.h"

#include "content/browser/android/media_player_manager_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/android/surface_callback.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "jni/BrowserProcessSurfaceTexture_jni.h"
#include "media/base/android/media_player_bridge.h"

using base::android::MethodID;

namespace content {

// Pass a java surface object to the MediaPlayerBridge object
// identified by render process handle, render view ID and player ID.
static void SetSurfacePeer(
    scoped_refptr<SurfaceTextureBridge> surface_texture_bridge,
    base::ProcessHandle render_process_handle,
    int render_view_id,
    int player_id) {
  int renderer_id = 0;
  RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
  while (!it.IsAtEnd()) {
    if (it.GetCurrentValue()->GetHandle() == render_process_handle) {
      renderer_id = it.GetCurrentValue()->GetID();
      break;
    }
    it.Advance();
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  if (renderer_id) {
    RenderViewHostImpl* host = RenderViewHostImpl::FromID(
        renderer_id, render_view_id);
    if (host) {
      media::MediaPlayerBridge* player =
          host->media_player_manager()->GetPlayer(player_id);
      if (player &&
          player != host->media_player_manager()->GetFullscreenPlayer()) {
        base::android::ScopedJavaLocalRef<jclass> cls(
            base::android::GetClass(env, "android/view/Surface"));
        jmethodID constructor = MethodID::Get<MethodID::TYPE_INSTANCE>(
            env, cls.obj(), "<init>", "(Landroid/graphics/SurfaceTexture;)V");
        ScopedJavaLocalRef<jobject> j_surface(
            env, env->NewObject(
                cls.obj(), constructor,
                surface_texture_bridge->j_surface_texture().obj()));
        player->SetVideoSurface(j_surface.obj());
        ReleaseSurface(j_surface.obj());
      }
    }
  }
}

SurfaceTexturePeerBrowserImpl::SurfaceTexturePeerBrowserImpl(
    bool player_in_render_process)
    : player_in_render_process_(player_in_render_process) {
}

SurfaceTexturePeerBrowserImpl::~SurfaceTexturePeerBrowserImpl() {
  if (surface_.obj())
    ReleaseSurface(surface_.obj());
}

void SurfaceTexturePeerBrowserImpl::EstablishSurfaceTexturePeer(
    base::ProcessHandle render_process_handle,
    SurfaceTextureTarget type,
    scoped_refptr<SurfaceTextureBridge> surface_texture_bridge,
    int render_view_id,
    int player_id) {
  if (!surface_texture_bridge)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  if (player_in_render_process_) {
    Java_BrowserProcessSurfaceTexture_establishSurfaceTexturePeer(
        env, render_process_handle, type,
        surface_texture_bridge->j_surface_texture().obj(),
        render_view_id, player_id);
  } else {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &SetSurfacePeer, surface_texture_bridge, render_process_handle,
        render_view_id, player_id));
  }
}

bool SurfaceTexturePeerBrowserImpl::RegisterBrowserProcessSurfaceTexture(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
