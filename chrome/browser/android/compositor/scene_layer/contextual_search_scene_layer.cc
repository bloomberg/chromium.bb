// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/contextual_search_scene_layer.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "cc/layers/solid_color_layer.h"
#include "chrome/browser/android/compositor/layer/contextual_search_layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/ContextualSearchSceneLayer_jni.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/gfx/android/java_bitmap.h"

namespace chrome {
namespace android {

ContextualSearchSceneLayer::ContextualSearchSceneLayer(JNIEnv* env,
                                                       jobject jobj)
    : SceneLayer(env, jobj),
      base_page_brightness_(1.0f),
      content_container_(cc::Layer::Create()) {
  // Responsible for moving the base page without modifying the layer itself.
  content_container_->SetIsDrawable(true);
  content_container_->SetPosition(gfx::PointF(0.0f, 0.0f));
  layer()->AddChild(content_container_);
}

void ContextualSearchSceneLayer::CreateContextualSearchLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jobject>& jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  contextual_search_layer_ = ContextualSearchLayer::Create(resource_manager);

  // The Contextual Search layer is initially invisible.
  contextual_search_layer_->layer()->SetHideLayerAndSubtree(true);

  layer()->AddChild(contextual_search_layer_->layer());
}

ContextualSearchSceneLayer::~ContextualSearchSceneLayer() {
}

void ContextualSearchSceneLayer::UpdateContextualSearchLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    jint search_bar_background_resource_id,
    jint search_context_resource_id,
    jint search_term_resource_id,
    jint search_caption_resource_id,
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
    jfloat base_page_brightness,
    jfloat base_page_offset,
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
    jfloat search_panel_x,
    jfloat search_panel_y,
    jfloat search_panel_width,
    jfloat search_panel_height,
    jfloat search_bar_margin_side,
    jfloat search_bar_height,
    jfloat search_context_opacity,
    jfloat search_term_opacity,
    jfloat search_caption_animation_percentage,
    jboolean search_caption_visible,
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
    jint progress_bar_completion) {
  // NOTE(pedrosimonetti): The ContentViewCore might not exist at this time if
  // the Contextual Search Result has not been requested yet. In this case,
  // we'll pass NULL to Contextual Search's Layer Tree.
  content::ContentViewCore* content_view_core =
      !jcontent_view_core ? NULL
                          : content::ContentViewCore::GetNativeContentViewCore(
                                env, jcontent_view_core);

  // Fade the base page out.
  if (base_page_brightness_ != base_page_brightness) {
    base_page_brightness_ = base_page_brightness;
    cc::FilterOperations filters;
    if (base_page_brightness < 1.f) {
      filters.Append(
          cc::FilterOperation::CreateBrightnessFilter(base_page_brightness));
    }
    content_container_->SetFilters(filters);
  }

  // Move the base page contents up.
  content_container_->SetPosition(gfx::PointF(0.0f, base_page_offset));

  contextual_search_layer_->SetProperties(
      search_bar_background_resource_id,
      search_context_resource_id,
      search_term_resource_id,
      search_caption_resource_id,
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
      search_panel_x,
      search_panel_y,
      search_panel_width,
      search_panel_height,
      search_bar_margin_side,
      search_bar_height,
      search_context_opacity,
      search_term_opacity,
      search_caption_animation_percentage,
      search_caption_visible,
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

  // Make the layer visible if it is not already.
  contextual_search_layer_->layer()->SetHideLayerAndSubtree(false);
}

void ContextualSearchSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (!content_tree || !content_tree->layer()) return;

  if (!content_tree->layer()->parent()
      || (content_tree->layer()->parent()->id() != content_container_->id())) {
    content_container_->AddChild(content_tree->layer());
  }
}

void ContextualSearchSceneLayer::HideTree(JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  // TODO(mdjones): Create super class for this logic.
  if (contextual_search_layer_) {
    contextual_search_layer_->layer()->SetHideLayerAndSubtree(true);
  }
  // Reset base page brightness.
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateBrightnessFilter(1.0f));
  content_container_->SetFilters(filters);
  // Reset base page offset.
  content_container_->SetPosition(gfx::PointF(0.0f, 0.0f));
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
