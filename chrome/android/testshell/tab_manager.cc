// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/testshell/tab_manager.h"

#include "base/logging.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "chrome/browser/android/tab_base_android_impl.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/draw_delegate.h"
#include "jni/TabManager_jni.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"

#include <android/native_window_jni.h>

using base::android::ScopedJavaLocalRef;

namespace {

struct GlobalState {
  scoped_ptr<content::Compositor> compositor;
  scoped_ptr<WebKit::WebLayer> root_layer;
};

base::LazyInstance<GlobalState> g_global_state = LAZY_INSTANCE_INITIALIZER;

content::Compositor* GetCompositor() {
  return g_global_state.Get().compositor.get();
}

static void SurfacePresented(
    const content::DrawDelegate::SurfacePresentedCallback& callback,
    uint32 sync_point) {
  callback.Run(sync_point);
}

void DummyCallback(uint32) { }

static void SurfaceUpdated(
    uint64 texture,
    content::RenderWidgetHostView* view,
    const content::DrawDelegate::SurfacePresentedCallback& callback) {
  GetCompositor()->OnSurfaceUpdated(base::Bind(
      &SurfacePresented, callback));
}

}  // anonymous namespace

namespace chrome {

// Register native methods
bool RegisterTabManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void Init(JNIEnv* env, jclass clazz, jobject obj) {
  content::DrawDelegate::SurfaceUpdatedCallback cb = base::Bind(
      &SurfaceUpdated);
  content::DrawDelegate::GetInstance()->SetUpdateCallback(cb);
  if (!GetCompositor()) {
    content::Compositor::Initialize();
    g_global_state.Get().compositor.reset(content::Compositor::Create());
    DCHECK(!g_global_state.Get().root_layer.get());
    g_global_state.Get().root_layer.reset(WebKit::WebLayer::create());
  }
}

static void SurfaceCreated(JNIEnv* env, jclass clazz, jobject jsurface) {
  ANativeWindow* native_window = ANativeWindow_fromSurface(env, jsurface);
  if (native_window) {
    GetCompositor()->SetWindowSurface(native_window);
    ANativeWindow_release(native_window);
    GetCompositor()->SetRootLayer(g_global_state.Get().root_layer.get());
  }
}

static void SurfaceDestroyed(JNIEnv* env, jclass clazz) {
  GetCompositor()->SetWindowSurface(NULL);
}

static void SurfaceSetSize(
    JNIEnv* env, jclass clazz, jint width, jint height) {
  gfx::Size size = gfx::Size(width, height);
  content::DrawDelegate::GetInstance()->SetBounds(size);
  GetCompositor()->SetWindowBounds(size);
}

static void ShowTab(JNIEnv* env, jclass clazz, jint jtab) {
  if (!GetCompositor())
    return;
  TabBaseAndroidImpl* tab = reinterpret_cast<TabBaseAndroidImpl*>(jtab);
  g_global_state.Get().root_layer->addChild(tab->tab_layer());
  GetCompositor()->OnSurfaceUpdated(base::Bind(&DummyCallback));
}

static void HideTab(JNIEnv* env, jclass clazz, jint jtab) {
  if (!GetCompositor())
    return;
  TabBaseAndroidImpl* tab = reinterpret_cast<TabBaseAndroidImpl*>(jtab);
  tab->tab_layer()->removeFromParent();
}

}  // namespace chrome
