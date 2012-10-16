// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/surface_callback.h"

#include <android/native_window_jni.h>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "ui/gl/android_native_window.h"
#include "jni/SurfaceCallback_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::GetClass;
using base::android::MethodID;
using base::WaitableEvent;
using content::SurfaceTexturePeer;

namespace content {

namespace {

struct GlobalState {
  base::Lock registration_lock;
  // We hold a reference to a message loop proxy which handles message loop
  // destruction gracefully, which is important since we post tasks from an
  // arbitrary binder thread while the main thread might be shutting down.
  // Also, in single-process mode we have two ChildThread objects for render
  // and gpu thread, so we need to store the msg loops separately.
  scoped_refptr<base::MessageLoopProxy> native_window_loop;
  scoped_refptr<base::MessageLoopProxy> video_surface_loop;
  NativeWindowCallback native_window_callback;
  VideoSurfaceCallback video_surface_callback;
};

base::LazyInstance<GlobalState>::Leaky g_state = LAZY_INSTANCE_INITIALIZER;

void RunNativeWindowCallback(int32 routing_id,
                             int32 renderer_id,
                             ANativeWindow* native_window,
                             WaitableEvent* completion) {
  g_state.Pointer()->native_window_callback.Run(
      routing_id, renderer_id, native_window, completion);
}

void RunVideoSurfaceCallback(int32 routing_id,
                             int32 renderer_id,
                             jobject surface) {
  g_state.Pointer()->video_surface_callback.Run(
      routing_id, renderer_id, surface);
}

}  // namespace <anonymous>

static void SetSurface(JNIEnv* env, jclass clazz,
                       jint type,
                       jobject surface,
                       jint primaryID,
                       jint secondaryID) {
  SetSurfaceAsync(env, surface,
                  static_cast<SurfaceTexturePeer::SurfaceTextureTarget>(type),
                  primaryID, secondaryID, NULL);
}

void ReleaseSurface(jobject surface) {
  if (surface == NULL)
    return;

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  ScopedJavaLocalRef<jclass> cls(GetClass(env, "android/view/Surface"));

  jmethodID method = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, cls.obj(), "release", "()V");

  env->CallVoidMethod(surface, method);
}

void SetSurfaceAsync(JNIEnv* env,
                     jobject jsurface,
                     SurfaceTexturePeer::SurfaceTextureTarget type,
                     int primary_id,
                     int secondary_id,
                     WaitableEvent* completion) {
  base::AutoLock lock(g_state.Pointer()->registration_lock);

  ANativeWindow* native_window = NULL;

  if (jsurface &&
      type != SurfaceTexturePeer::SET_VIDEO_SURFACE_TEXTURE) {
    native_window = ANativeWindow_fromSurface(env, jsurface);
    ReleaseSurface(jsurface);
  }
  GlobalState* const global_state = g_state.Pointer();

  switch (type) {
    case SurfaceTexturePeer::SET_GPU_SURFACE_TEXTURE: {
      // This should only be sent as a reaction to the renderer
      // activating compositing. If the GPU process crashes, we expect this
      // to be resent after the new thread is initialized.
      DCHECK(global_state->native_window_loop);
      global_state->native_window_loop->PostTask(
          FROM_HERE,
          base::Bind(&RunNativeWindowCallback,
              primary_id,
              static_cast<uint32_t>(secondary_id),
              native_window,
              completion));
      // ANativeWindow_release will be called in SetNativeWindow()
      break;
    }
    case SurfaceTexturePeer::SET_VIDEO_SURFACE_TEXTURE: {
      jobject surface = env->NewGlobalRef(jsurface);
      DCHECK(global_state->video_surface_loop);
      global_state->video_surface_loop->PostTask(
          FROM_HERE,
          base::Bind(&RunVideoSurfaceCallback,
                     primary_id,
                     secondary_id,
                     surface));
      break;
    }
  }
}

void RegisterVideoSurfaceCallback(base::MessageLoopProxy* loop,
                                  VideoSurfaceCallback& callback) {
  GlobalState* const global_state = g_state.Pointer();
  base::AutoLock lock(global_state->registration_lock);
  global_state->video_surface_loop = loop;
  global_state->video_surface_callback = callback;
}

void RegisterNativeWindowCallback(base::MessageLoopProxy* loop,
                                  NativeWindowCallback& callback) {
  GlobalState* const global_state = g_state.Pointer();
  base::AutoLock lock(global_state->registration_lock);
  global_state->native_window_loop = loop;
  global_state->native_window_callback = callback;
}

bool RegisterSurfaceCallback(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
