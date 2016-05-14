// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/static_tab_scene_layer.h"

#include "cc/layers/layer.h"
#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "chrome/browser/android/compositor/layer/toolbar_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "content/public/browser/android/compositor.h"
#include "jni/StaticTabSceneLayer_jni.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace chrome {
namespace android {

StaticTabSceneLayer::StaticTabSceneLayer(JNIEnv* env, jobject jobj)
    : SceneLayer(env, jobj),
      last_set_tab_id_(-1),
      background_color_(SK_ColorWHITE),
      brightness_(1.f) {
}

StaticTabSceneLayer::~StaticTabSceneLayer() {
}

bool StaticTabSceneLayer::ShouldShowBackground() {
  scoped_refptr<cc::Layer> parent = layer_->parent();
  return parent.get() && parent->bounds() != layer_->bounds();
}

SkColor StaticTabSceneLayer::GetBackgroundColor() {
  return background_color_;
}

void StaticTabSceneLayer::UpdateTabLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jfloat content_viewport_x,
    jfloat content_viewport_y,
    jfloat content_viewport_width,
    jfloat content_viewport_height,
    const JavaParamRef<jobject>& jtab_content_manager,
    jint id,
    jint toolbar_resource_id,
    jboolean can_use_live_layer,
    jint default_background_color,
    jfloat x,
    jfloat y,
    jfloat width,
    jfloat height,
    jfloat content_offset_y,
    jfloat static_to_view_blend,
    jfloat saturation,
    jfloat brightness) {
  background_color_ = default_background_color;
  gfx::Size content_viewport_size(content_viewport_width,
                                  content_viewport_height);
  gfx::Point content_viewport_offset(content_viewport_x, content_viewport_y);
  if (!content_layer_.get()) {
    chrome::android::TabContentManager* tab_content_manager =
        chrome::android::TabContentManager::FromJavaObject(
            jtab_content_manager);
    content_layer_ = chrome::android::ContentLayer::Create(tab_content_manager);
    layer_->AddChild(content_layer_->layer());
  }

  // Only override the alpha of content layers when the static tab is first
  // assigned to the layer tree.
  float content_alpha_override = 1.f;
  bool should_override_content_alpha = last_set_tab_id_ != id;
  last_set_tab_id_ = id;

  // Set up the content layer and move it to the proper position.
  content_layer_->layer()->SetBounds(gfx::Size(width, height));
  content_layer_->layer()->SetPosition(gfx::PointF(x, y));
  content_layer_->SetProperties(
      id, can_use_live_layer, static_to_view_blend,
      should_override_content_alpha, content_alpha_override, saturation,
      gfx::Rect(content_viewport_size), content_viewport_size);

  gfx::Size content_bounds(0, 0);
  content_bounds = content_layer_->layer()->bounds();

  gfx::Size actual_content_size(content_layer_->GetContentSize());

  bool view_and_content_have_same_orientation =
      (content_viewport_size.width() > content_viewport_size.height()) ==
          (actual_content_size.width() > actual_content_size.height()) &&
      actual_content_size.width() > 0 && actual_content_size.height() > 0 &&
      content_viewport_size.width() > 0 && content_viewport_size.height() > 0;

  // This may not be true for frames during rotation.
  bool content_has_consistent_width =
      actual_content_size.width() == content_bounds.width() &&
      actual_content_size.width() == content_viewport_size.width();

  if (view_and_content_have_same_orientation ||
      (content_has_consistent_width && content_layer_->ShowingLiveLayer())) {
    y += content_offset_y;
  } else {
    // If our orientations are off and we have a static texture, or if we have
    // a live layer of an unexpected width, move the texture in by the
    // appropriate amount.
    x += content_viewport_offset.x();
    y += content_viewport_offset.y();
  }

  content_layer_->layer()->SetPosition(gfx::PointF(x, y));
  content_layer_->layer()->SetIsDrawable(true);

  // Only applies the brightness filter if the value has changed and is less
  // than 1.
  if (brightness != brightness_) {
    brightness_ = brightness;
    cc::FilterOperations filters;
    if (brightness_ < 1.f)
      filters.Append(cc::FilterOperation::CreateBrightnessFilter(brightness_));
    layer_->SetFilters(filters);
  }
}

void StaticTabSceneLayer::UpdateToolbarLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jobject>& jresource_manager,
    jint toolbar_resource_id,
    jint toolbar_background_color,
    jint url_bar_resource_id,
    jfloat url_bar_alpha,
    jfloat top_offset,
    bool visible,
    bool show_shadow) {
  // If the toolbar layer has not been created yet, create it.
  if (!toolbar_layer_) {
    ui::ResourceManager* resource_manager =
        ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
    toolbar_layer_ = ToolbarLayer::Create(resource_manager);
    toolbar_layer_->layer()->SetHideLayerAndSubtree(true);
    layer_->AddChild(toolbar_layer_->layer());
  }

  toolbar_layer_->layer()->SetHideLayerAndSubtree(!visible);
  if (visible) {
    toolbar_layer_->layer()->SetPosition(gfx::PointF(0, top_offset));
    // If we're at rest, hide the shadow.  The Android view should be drawing.
    bool clip_shadow = top_offset >= 0.f && !show_shadow;
    toolbar_layer_->PushResource(toolbar_resource_id, toolbar_background_color,
                                 false, SK_ColorWHITE, url_bar_resource_id,
                                 url_bar_alpha, false, clip_shadow);
  }
}

void StaticTabSceneLayer::UpdateProgressBar(JNIEnv* env,
                                       const JavaParamRef<jobject>& object,
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
  if (!toolbar_layer_) return;
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

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  StaticTabSceneLayer* scene_layer = new StaticTabSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

bool RegisterStaticTabSceneLayer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
