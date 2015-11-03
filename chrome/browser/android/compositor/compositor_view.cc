// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/compositor_view.h"

#include <vector>

#include <android/bitmap.h>
#include <android/native_window_jni.h>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_lists.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "chrome/browser/android/compositor/layer/toolbar_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "jni/CompositorView_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"

namespace chrome {
namespace android {

jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           jboolean low_mem_device,
           jint empty_background_color,
           jlong native_window_android,
           const JavaParamRef<jobject>& jlayer_title_cache,
           const JavaParamRef<jobject>& jtab_content_manager) {
  CompositorView* view;
  ui::WindowAndroid* window_android =
      reinterpret_cast<ui::WindowAndroid*>(native_window_android);
  LayerTitleCache* layer_title_cache =
      LayerTitleCache::FromJavaObject(jlayer_title_cache);
  TabContentManager* tab_content_manager =
      TabContentManager::FromJavaObject(jtab_content_manager);

  DCHECK(tab_content_manager);

  // TODO(clholgat): Remove the compositor tabstrip flag.
  view = new CompositorView(env, obj, empty_background_color, low_mem_device,
                            window_android, layer_title_cache,
                            tab_content_manager);

  ui::UIResourceProvider* ui_resource_provider = view->GetUIResourceProvider();
  // TODO(dtrainor): Pass the ResourceManager on the Java side to the tree
  // builders instead.
  if (layer_title_cache)
    layer_title_cache->SetResourceManager(view->GetResourceManager());
  if (tab_content_manager)
    tab_content_manager->SetUIResourceProvider(ui_resource_provider);

  return reinterpret_cast<intptr_t>(view);
}

CompositorView::CompositorView(JNIEnv* env,
                               jobject obj,
                               jint empty_background_color,
                               jboolean low_mem_device,
                               ui::WindowAndroid* window_android,
                               LayerTitleCache* layer_title_cache,
                               TabContentManager* tab_content_manager)
    : layer_title_cache_(layer_title_cache),
      tab_content_manager_(tab_content_manager),
      root_layer_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      scene_layer_(nullptr),
      current_surface_format_(0),
      content_width_(0),
      content_height_(0),
      overdraw_bottom_height_(0),
      overlay_video_mode_(false),
      empty_background_color_(empty_background_color),
      weak_factory_(this) {
  content::BrowserChildProcessObserver::Add(this);
  obj_.Reset(env, obj);
  compositor_.reset(content::Compositor::Create(this, window_android));

  toolbar_layer_ = ToolbarLayer::Create(&(compositor_->GetResourceManager()));

  root_layer_->SetIsDrawable(true);
  root_layer_->SetBackgroundColor(SK_ColorWHITE);

  toolbar_layer_->layer()->SetHideLayerAndSubtree(true);
  root_layer_->AddChild(toolbar_layer_->layer());
}

CompositorView::~CompositorView() {
  content::BrowserChildProcessObserver::Remove(this);
  tab_content_manager_->OnUIResourcesWereEvicted();

  // Explicitly reset these scoped_ptrs here because otherwise we callbacks will
  // try to access member variables during destruction.
  compositor_.reset(NULL);
}

void CompositorView::Destroy(JNIEnv* env, jobject object) {
  delete this;
}

ui::ResourceManager* CompositorView::GetResourceManager() {
  if (!compositor_)
    return NULL;
  return &compositor_->GetResourceManager();
}

base::android::ScopedJavaLocalRef<jobject> CompositorView::GetResourceManager(
    JNIEnv* env,
    jobject jobj) {
  return compositor_->GetResourceManager().GetJavaObject();
}

void CompositorView::UpdateLayerTreeHost() {
  JNIEnv* env = base::android::AttachCurrentThread();
  // TODO(wkorman): Rename JNI interface to onCompositorUpdateLayerTreeHost.
  Java_CompositorView_onCompositorLayout(env, obj_.obj());
}

void CompositorView::OnSwapBuffersCompleted(int pending_swap_buffers) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CompositorView_onSwapBuffersCompleted(env, obj_.obj(),
                                             pending_swap_buffers);
}

ui::UIResourceProvider* CompositorView::GetUIResourceProvider() {
  if (!compositor_)
    return NULL;
  return &compositor_->GetUIResourceProvider();
}

void CompositorView::SurfaceCreated(JNIEnv* env, jobject object) {
  compositor_->SetRootLayer(root_layer_);
  current_surface_format_ = 0;
}

void CompositorView::SurfaceDestroyed(JNIEnv* env, jobject object) {
  compositor_->SetSurface(NULL);
  current_surface_format_ = 0;
  tab_content_manager_->OnUIResourcesWereEvicted();
}

void CompositorView::SurfaceChanged(JNIEnv* env,
                                    jobject object,
                                    jint format,
                                    jint width,
                                    jint height,
                                    jobject surface) {
  DCHECK(surface);
  if (current_surface_format_ != format) {
    current_surface_format_ = format;
    compositor_->SetSurface(surface);
  }
  gfx::Size size = gfx::Size(width, height);
  compositor_->SetWindowBounds(size);
  content_width_ = size.width();
  content_height_ = size.height();
  root_layer_->SetBounds(gfx::Size(content_width_, content_height_));
}

void CompositorView::SetLayoutViewport(JNIEnv* env,
                                       jobject object,
                                       jfloat x,
                                       jfloat y,
                                       jfloat width,
                                       jfloat height,
                                       jfloat visible_x_offset,
                                       jfloat visible_y_offset,
                                       jfloat overdraw_bottom_height,
                                       jfloat dp_to_pixel) {
  overdraw_bottom_height_ = overdraw_bottom_height;
  compositor_->setDeviceScaleFactor(dp_to_pixel);
  root_layer_->SetBounds(gfx::Size(content_width_, content_height_));
}

void CompositorView::SetBackground(bool visible, SkColor color) {
  if (overlay_video_mode_)
    visible = false;
  root_layer_->SetBackgroundColor(color);
  root_layer_->SetIsDrawable(visible);
}

void CompositorView::SetOverlayVideoMode(JNIEnv* env,
                                         jobject object,
                                         bool enabled) {
  if (overlay_video_mode_ == enabled)
    return;
  overlay_video_mode_ = enabled;
  compositor_->SetHasTransparentBackground(enabled);
  SetNeedsComposite(env, object);
}

void CompositorView::SetSceneLayer(JNIEnv* env,
                                   jobject object,
                                   jobject jscene_layer) {
  SceneLayer* scene_layer = SceneLayer::FromJavaObject(env, jscene_layer);

  if (scene_layer_ != scene_layer) {
    // Old tree provider is being detached.
    if (scene_layer_ != nullptr)
      scene_layer_->OnDetach();

    scene_layer_ = scene_layer;

    if (scene_layer == nullptr) {
      scene_layer_layer_ = nullptr;
      return;
    }

    scene_layer_layer_ = scene_layer->layer();
    root_layer_->InsertChild(scene_layer->layer(), 0);
  }

  if (scene_layer) {
    SetBackground(scene_layer->ShouldShowBackground(),
                  scene_layer->GetBackgroundColor());
  } else {
#ifndef NDEBUG
    // This should not happen. Setting red background just for debugging.
    SetBackground(true, SK_ColorRED);
#else
    SetBackground(true, SK_ColorBLACK);
#endif
  }
}

int CompositorView::GetUsableContentHeight() {
  return std::max(content_height_ - overdraw_bottom_height_, 0);
}

void CompositorView::UpdateToolbarLayer(JNIEnv* env,
                                        jobject object,
                                        jint toolbar_resource_id,
                                        jint toolbar_background_color,
                                        jint url_bar_resource_id,
                                        jfloat url_bar_alpha,
                                        jfloat top_offset,
                                        jfloat brightness,
                                        bool visible,
                                        bool show_shadow) {
  toolbar_layer_->layer()->SetHideLayerAndSubtree(!visible);
  if (visible) {
    toolbar_layer_->layer()->SetPosition(gfx::PointF(0, top_offset));
    // If we're at rest, hide the shadow.  The Android view should be drawing.
    bool clip_shadow = top_offset >= 0.f && !show_shadow;
    toolbar_layer_->PushResource(toolbar_resource_id, toolbar_background_color,
                                 false, SK_ColorWHITE, url_bar_resource_id,
                                 url_bar_alpha, false, brightness, clip_shadow);
  }
}

void CompositorView::UpdateProgressBar(JNIEnv* env,
                                       jobject object,
                                       jint progress_bar_x,
                                       jint progress_bar_y,
                                       jint progress_bar_width,
                                       jint progress_bar_height,
                                       jint progress_bar_color,
                                       jint progress_bar_background_x,
                                       jint progress_bar_background_y,
                                       jint progress_bar_background_width,
                                       jint progress_bar_background_height,
                                       jint progress_bar_background_color) {
  toolbar_layer_->UpdateProgressBar(progress_bar_x,
                                    progress_bar_y,
                                    progress_bar_width,
                                    progress_bar_height,
                                    progress_bar_color,
                                    progress_bar_background_x,
                                    progress_bar_background_y,
                                    progress_bar_background_width,
                                    progress_bar_background_height,
                                    progress_bar_background_color);
}

void CompositorView::FinalizeLayers(JNIEnv* env, jobject jobj) {
  UNSHIPPED_TRACE_EVENT0("compositor", "CompositorView::FinalizeLayers");
}

void CompositorView::SetNeedsComposite(JNIEnv* env, jobject object) {
  compositor_->SetNeedsComposite();
}

void CompositorView::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  LOG(WARNING) << "Child process disconnected (type=" << data.process_type
               << ") pid=" << data.handle << ")";
  if (base::android::BuildInfo::GetInstance()->sdk_int() <=
          base::android::SDK_VERSION_JELLY_BEAN_MR2 &&
      data.process_type == content::PROCESS_TYPE_GPU) {
    JNIEnv* env = base::android::AttachCurrentThread();
    compositor_->SetSurface(nullptr);
    Java_CompositorView_onJellyBeanSurfaceDisconnectWorkaround(
        env, obj_.obj(), overlay_video_mode_);
  }
}

void CompositorView::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    int exit_code) {
  // The Android TERMINATION_STATUS_OOM_PROTECTED hack causes us to never go
  // through here but through BrowserChildProcessHostDisconnected() instead.
}

// Register native methods
bool RegisterCompositorView(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
