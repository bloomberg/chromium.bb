// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTEXTUAL_SEARCH_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTEXTUAL_SEARCH_SCENE_LAYER_H_

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"

namespace cc {
class Layer;
}

namespace android {

class ContextualSearchLayer;

class ContextualSearchSceneLayer : public SceneLayer,
    public chrome::BitmapFetcherDelegate {
 public:
  ContextualSearchSceneLayer(JNIEnv* env, jobject jobj);
  ~ContextualSearchSceneLayer() override;

  void CreateContextualSearchLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void UpdateContextualSearchLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint search_bar_background_resource_id,
      jint search_context_resource_id,
      jint search_term_resource_id,
      jint search_caption_resource_id,
      jint search_bar_shadow_resource_id,
      jint search_provider_icon_resource_id,
      jint quick_action_icon_resource_id,
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
      const base::android::JavaParamRef<jobject>& jweb_contents,
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
      jfloat search_text_layer_min_height,
      jfloat search_term_opacity,
      jfloat search_term_caption_spacing,
      jfloat search_caption_animation_percentage,
      jboolean search_caption_visible,
      jboolean search_bar_border_visible,
      jfloat search_bar_border_height,
      jboolean search_bar_shadow_visible,
      jfloat search_bar_shadow_opacity,
      jboolean quick_action_icon_visible,
      jboolean thumbnail_visible,
      jstring j_thumbnail_url,
      jfloat custom_image_visibility_percentage,
      jint bar_image_size,
      jfloat arrow_icon_opacity,
      jfloat arrow_icon_rotation,
      jfloat close_icon_opacity,
      jboolean progress_bar_visible,
      jfloat progress_bar_height,
      jfloat progress_bar_opacity,
      jint progress_bar_completion,
      jfloat divider_line_visibility_percentage,
      jfloat divider_line_width,
      jfloat divider_line_height,
      jint divider_line_color,
      jfloat divider_line_x_offset,
      jboolean touch_highlight_visible,
      jfloat touch_highlight_x_offset,
      jfloat touch_highlight_width,
      jint bar_handle_resource_id,
      jfloat bar_handle_offset_y,
      jfloat bar_padding_bottom,
      jobject j_profile);

  // Inherited from BitmapFetcherDelegate
  void OnFetchComplete(
      const GURL& url,
      const SkBitmap* bitmap) override;

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  void HideTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj);

 private:
  void FetchThumbnail(jobject j_profile);

  JNIEnv* env_;
  base::android::ScopedJavaGlobalRef<jobject> object_;
  float base_page_brightness_;
  std::string thumbnail_url_;
  std::unique_ptr<chrome::BitmapFetcher> fetcher_;

  scoped_refptr<ContextualSearchLayer> contextual_search_layer_;
  scoped_refptr<cc::Layer> content_container_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchSceneLayer);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTEXTUAL_SEARCH_SCENE_LAYER_H_
