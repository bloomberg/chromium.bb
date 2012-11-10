// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_render_view.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/content_view_layer_renderer.h"
#include "content/public/browser/android/draw_delegate.h"
#include "jni/ContentViewRenderView_jni.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "ui/gfx/size.h"

#include <android/native_window_jni.h>

using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;
using content::Compositor;
using content::DrawDelegate;

namespace {

class CompositorClient : public Compositor::Client {
 public:
  virtual void ScheduleComposite() OVERRIDE;
};

class ContentViewLayerRendererImpl;

struct GlobalState {
  GlobalState() : g_scheduled_composite(false) {}
  ScopedJavaGlobalRef<jobject> j_obj;
  CompositorClient client;
  scoped_ptr<content::Compositor> compositor;
  scoped_ptr<WebKit::WebLayer> root_layer;
  scoped_ptr<ContentViewLayerRendererImpl> layer_renderer;
  bool g_scheduled_composite;
};

base::LazyInstance<GlobalState> g_global_state = LAZY_INSTANCE_INITIALIZER;

content::Compositor* GetCompositor() {
  return g_global_state.Get().compositor.get();
}

class ContentViewLayerRendererImpl : public content::ContentViewLayerRenderer {
 public:
  ContentViewLayerRendererImpl() {}
  virtual ~ContentViewLayerRendererImpl() {}

 private:
  virtual void AttachLayer(WebKit::WebLayer* layer) OVERRIDE {
    if (GetCompositor())
      g_global_state.Get().root_layer->addChild(layer);
  }

  virtual void DetachLayer(WebKit::WebLayer* layer) OVERRIDE {
    if (GetCompositor())
      layer->removeFromParent();
  }

  DISALLOW_COPY_AND_ASSIGN(ContentViewLayerRendererImpl);
};

void Composite() {
  g_global_state.Get().g_scheduled_composite = false;
  if (GetCompositor())
    GetCompositor()->Composite();
}

void CompositorClient::ScheduleComposite() {
  if (!g_global_state.Get().g_scheduled_composite) {
    g_global_state.Get().g_scheduled_composite = true;
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&Composite));
  }
}

void InitCompositor() {
  if (GetCompositor())
    return;

  Compositor::Initialize();
  g_global_state.Get().compositor.reset(
      Compositor::Create(&g_global_state.Get().client));
  DCHECK(!g_global_state.Get().root_layer.get());
  g_global_state.Get().root_layer.reset(WebKit::WebLayer::create());
  g_global_state.Get().layer_renderer.reset(new ContentViewLayerRendererImpl());
}

} // anonymous namespace

namespace content {

// Register native methods
bool RegisterContentViewRenderView(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void Init(JNIEnv* env, jobject obj) {
  g_global_state.Get().j_obj.Reset(ScopedJavaLocalRef<jobject>(env, obj));
}

static jint GetNativeContentViewLayerRenderer(JNIEnv* env, jclass clazz) {
  // The compositor might not have been initialized yet.
  InitCompositor();
  // Note it's important to static cast to the interface here as we'll
  // reinterpret cast the jint back to the interace later on.
  return reinterpret_cast<jint>(static_cast<content::ContentViewLayerRenderer*>(
      g_global_state.Get().layer_renderer.get()));
}

static void SurfaceCreated(
    JNIEnv* env, jclass clazz, jobject jsurface) {
  InitCompositor();
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
  DrawDelegate::GetInstance()->SetBounds(size);
  GetCompositor()->SetWindowBounds(size);
}

}  // namespace content
