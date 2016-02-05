// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTEXTUAL_SEARCH_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTEXTUAL_SEARCH_SCENE_LAYER_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"

namespace chrome {
namespace android {

class ContextualSearchLayer;

class ContextualSearchSceneLayer : public SceneLayer {
 public:
  ContextualSearchSceneLayer(JNIEnv* env, jobject jobj);
  ~ContextualSearchSceneLayer() override;

  void UpdateContextualSearchLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
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
      const base::android::JavaParamRef<jobject>& jcontent_view_core,
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
      const base::android::JavaParamRef<jobject>& jresource_manager);

 private:
  scoped_refptr<ContextualSearchLayer> contextual_search_layer_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchSceneLayer);
};

bool RegisterContextualSearchSceneLayer(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_CONTEXTUAL_SEARCH_SCENE_LAYER_H_
