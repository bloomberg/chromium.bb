// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/contextual_search_scene_layer.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "chrome/browser/android/compositor/layer/contextual_search_layer.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/ContextualSearchSceneLayer_jni.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/gfx/android/java_bitmap.h"

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
    const JavaParamRef<jobject>& object,
    jint search_bar_background_resource_id,
    jint search_context_resource_id,
    jint search_term_resource_id,
    jint search_bar_shadow_resource_id,
    jint panel_icon_resource_id,
    jint search_provider_icon_sprite_metadata_resource_id,
    jint arrow_up_resource_id,
    jint close_icon_resource_id,
    jint progress_bar_background_resource_id,
    jint progress_bar_resource_id,
    jint search_promo_resource_id,
    jint peek_promo_ripple_resource_id,
    jint peek_promo_text_resource_id,
    jfloat dp_to_px,
    const JavaParamRef<jobject>& jcontent_view_core,
    jboolean search_promo_visible,
    jfloat search_promo_height,
    jfloat search_promo_opacity,
    jboolean search_peek_promo_visible,
    jfloat search_peek_promo_height,
    jfloat search_peek_promo_padding,
    jfloat search_peek_promo_ripple_width,
    jfloat search_peek_promo_ripple_opacity,
    jfloat search_peek_promo_text_opacity,
    jfloat search_panel_X,
    jfloat search_panel_y,
    jfloat search_panel_width,
    jfloat search_panel_height,
    jfloat search_bar_margin_side,
    jfloat search_bar_height,
    jfloat search_context_opacity,
    jfloat search_term_opacity,
    jboolean search_bar_border_visible,
    jfloat search_bar_border_height,
    jboolean search_bar_shadow_visible,
    jfloat search_bar_shadow_opacity,
    jboolean search_provider_icon_sprite_visible,
    jfloat search_provider_icon_sprite_completion_percentage,
    jfloat arrow_icon_opacity,
    jfloat arrow_icon_rotation,
    jfloat close_icon_opacity,
    jboolean progress_bar_visible,
    jfloat progress_bar_height,
    jfloat progress_bar_opacity,
    jint progress_bar_completion,
    const JavaParamRef<jobject>& jresource_manager) {
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
      search_context_resource_id,
      search_term_resource_id,
      search_bar_shadow_resource_id,
      panel_icon_resource_id,
      search_provider_icon_sprite_metadata_resource_id,
      arrow_up_resource_id,
      close_icon_resource_id,
      progress_bar_background_resource_id,
      progress_bar_resource_id,
      search_promo_resource_id,
      peek_promo_ripple_resource_id,
      peek_promo_text_resource_id,
      dp_to_px,
      content_view_core,
      search_promo_visible,
      search_promo_height,
      search_promo_opacity,
      search_peek_promo_visible,
      search_peek_promo_height,
      search_peek_promo_padding,
      search_peek_promo_ripple_width,
      search_peek_promo_ripple_opacity,
      search_peek_promo_text_opacity,
      search_panel_X,
      search_panel_y,
      search_panel_width,
      search_panel_height,
      search_bar_margin_side,
      search_bar_height,
      search_context_opacity,
      search_term_opacity,
      search_bar_border_visible,
      search_bar_border_height,
      search_bar_shadow_visible,
      search_bar_shadow_opacity,
      search_provider_icon_sprite_visible,
      search_provider_icon_sprite_completion_percentage,
      arrow_icon_opacity,
      arrow_icon_rotation,
      close_icon_opacity,
      progress_bar_visible,
      progress_bar_height,
      progress_bar_opacity,
      progress_bar_completion);
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
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
