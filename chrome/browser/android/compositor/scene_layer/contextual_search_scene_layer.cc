// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/contextual_search_scene_layer.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/compositor/layer/contextual_search_layer.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/ContextualSearchSceneLayer_jni.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace chrome {
namespace android {

ContextualSearchSceneLayer::ContextualSearchSceneLayer(JNIEnv* env,
                                                       jobject jobj)
    : SceneLayer(env, jobj) {
}

ContextualSearchSceneLayer::~ContextualSearchSceneLayer() {
}

void ContextualSearchSceneLayer::UpdateContextualSearchLayer(
    JNIEnv* env,
    jobject object,
    jint search_bar_background_resource_id,
    jint search_bar_text_resource_id,
    jint search_bar_shadow_resource_id,
    jint search_provider_icon_resource_id,
    jint search_icon_resource_id,
    jint arrow_up_resource_id,
    jint progress_bar_background_resource_id,
    jint progress_bar_resource_id,
    jint search_promo_resource_id,
    jobject jcontent_view_core,
    jboolean search_promo_visible,
    jfloat search_promo_height,
    jfloat search_promo_opacity,
    jfloat search_panel_y,
    jfloat search_panel_width,
    jfloat search_bar_margin_top,
    jfloat search_bar_margin_side,
    jfloat search_bar_height,
    jfloat search_bar_text_opacity,
    jboolean search_bar_border_visible,
    jfloat search_bar_border_y,
    jfloat search_bar_border_height,
    jboolean search_bar_shadow_visible,
    jfloat search_bar_shadow_opacity,
    jfloat search_provider_icon_opacity,
    jfloat search_icon_opacity,
    jboolean arrow_icon_visible,
    jfloat arrow_icon_opacity,
    jfloat arrow_icon_rotation,
    jboolean progress_bar_visible,
    jfloat progress_bar_y,
    jfloat progress_bar_height,
    jfloat progress_bar_opacity,
    jint progress_bar_completion,
    jobject jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  // Lazily construct the contextual search layer, as the feature is only
  // conditionally enabled.
  if (!contextual_search_layer_.get()) {
    if (!resource_manager)
      return;
    contextual_search_layer_ = ContextualSearchLayer::Create(resource_manager);
    layer_->AddChild(contextual_search_layer_->layer());
  }

  // NOTE(pedrosimonetti): The ContentViewCore might not exist at this time if
  // the Contextual Search Result has not been requested yet. In this case,
  // we'll pass NULL to Contextual Search's Layer Tree.
  content::ContentViewCore* content_view_core =
      !jcontent_view_core ? NULL
                          : content::ContentViewCore::GetNativeContentViewCore(
                                env, jcontent_view_core);

  contextual_search_layer_->SetProperties(
      search_bar_background_resource_id,
      search_bar_text_resource_id,
      search_bar_shadow_resource_id,
      search_provider_icon_resource_id,
      search_icon_resource_id,
      arrow_up_resource_id,
      progress_bar_background_resource_id,
      progress_bar_resource_id,
      search_promo_resource_id,
      content_view_core,
      search_promo_visible,
      search_promo_height,
      search_promo_opacity,
      search_panel_y,
      search_panel_width,
      search_bar_margin_top,
      search_bar_margin_side,
      search_bar_height,
      search_bar_text_opacity,
      search_bar_border_visible,
      search_bar_border_y,
      search_bar_border_height,
      search_bar_shadow_visible,
      search_bar_shadow_opacity,
      search_provider_icon_opacity,
      search_icon_opacity,
      arrow_icon_visible,
      arrow_icon_opacity,
      arrow_icon_rotation,
      progress_bar_visible,
      progress_bar_y,
      progress_bar_height,
      progress_bar_opacity,
      progress_bar_completion);
}

static jlong Init(JNIEnv* env, jobject jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  ContextualSearchSceneLayer* tree_provider =
      new ContextualSearchSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(tree_provider);
}

bool RegisterContextualSearchSceneLayer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
