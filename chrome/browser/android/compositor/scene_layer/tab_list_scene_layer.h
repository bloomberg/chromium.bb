// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_LIST_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_LIST_SCENE_LAYER_H_

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "cc/layers/layer.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {
class ResourceManager;
}

namespace chrome {
namespace android {

class LayerTitleCache;
class TabContentManager;
class TabLayer;

class TabListSceneLayer : public SceneLayer {
 public:
  TabListSceneLayer(JNIEnv* env, jobject jobj);
  ~TabListSceneLayer() override;

  // TODO(changwan): remove this once we have refactored
  // ContextualSearchSupportedLayout into LayoutHelper.
  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  void BeginBuildingFrame(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jobj);
  void FinishBuildingFrame(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jobj);
  void UpdateLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint background_color,
      jfloat viewport_x,
      jfloat viewport_y,
      jfloat viewport_width,
      jfloat viewport_height,
      const base::android::JavaParamRef<jobject>& jlayer_title_cache,
      const base::android::JavaParamRef<jobject>& jtab_content_manager,
      const base::android::JavaParamRef<jobject>& jresource_manager);
  // TODO(dtrainor): This method is ridiculous.  Break this apart?
  void PutTabLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint id,
      jint toolbar_resource_id,
      jint close_button_resource_id,
      jint shadow_resource_id,
      jint contour_resource_id,
      jint back_logo_resource_id,
      jint border_resource_id,
      jint border_inner_shadow_resource_id,
      jboolean can_use_live_layer,
      jint tab_background_color,
      jint back_logo_color,
      jboolean incognito,
      jboolean is_portrait,
      jfloat x,
      jfloat y,
      jfloat width,
      jfloat height,
      jfloat content_width,
      jfloat content_height,
      jfloat visible_content_height,
      jfloat shadow_x,
      jfloat shadow_y,
      jfloat shadow_width,
      jfloat shadow_height,
      jfloat pivot_x,
      jfloat pivot_y,
      jfloat rotation_x,
      jfloat rotation_y,
      jfloat alpha,
      jfloat border_alpha,
      jfloat border_inner_shadow_alpha,
      jfloat contour_alpha,
      jfloat shadow_alpha,
      jfloat close_alpha,
      jfloat close_btn_width,
      jfloat static_to_view_blend,
      jfloat border_scale,
      jfloat saturation,
      jfloat brightness,
      jboolean show_toolbar,
      jint toolbar_background_color,
      jboolean anonymize_toolbar,
      jint toolbar_textbox_resource_id,
      jint toolbar_textbox_background_color,
      jfloat toolbar_textbox_alpha,
      jfloat toolbar_alpha,
      jfloat toolbar_y_offset,
      jfloat side_border_scale,
      jboolean attach_content,
      jboolean inset_border);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject(JNIEnv* env);

  void OnDetach() override;
  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

 private:
  void RemoveAllRemainingTabLayers();
  void RemoveTabLayersInRange(unsigned start_index, unsigned end_index);

  typedef std::vector<scoped_refptr<TabLayer>> TabLayerList;

  scoped_refptr<TabLayer> GetNextLayer(bool incognito);

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  bool content_obscures_self_;
  unsigned write_index_;
  ui::ResourceManager* resource_manager_;
  LayerTitleCache* layer_title_cache_;
  TabContentManager* tab_content_manager_;
  TabLayerList layers_;
  SkColor background_color_;

  // We need to make sure that content_tree_ is always in front of own_tree_.
  scoped_refptr<cc::Layer> own_tree_;
  scoped_refptr<cc::Layer> content_tree_;

  DISALLOW_COPY_AND_ASSIGN(TabListSceneLayer);
};

bool RegisterTabListSceneLayer(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_TAB_LIST_SCENE_LAYER_H_
