// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_READER_MODE_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_READER_MODE_SCENE_LAYER_H_

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace cc {
class Layer;
class SolidColorLayer;
}

namespace android {

class ReaderModeLayer;

class ReaderModeSceneLayer : public SceneLayer {
 public:
  ReaderModeSceneLayer(JNIEnv* env, jobject jobj);
  ~ReaderModeSceneLayer() override;

  void CreateReaderModeLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& jresource_manager);

  void SetResourceIds(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint text_resource_id,
      jint bar_background_resource_id,
      jint bar_shadow_resource_id,
      jint panel_icon_resource_id,
      jint close_icon_resource_id);

  void Update(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jfloat dp_to_px,
      jfloat base_page_brightness,
      jfloat base_page_offset,
      const base::android::JavaParamRef<jobject>& jcontent_view_core,
      jfloat panel_X,
      jfloat panel_y,
      jfloat panel_width,
      jfloat panel_height,
      jfloat bar_margin_side,
      jfloat bar_height,
      jfloat text_opacity,
      jboolean bar_border_visible,
      jfloat bar_border_height,
      jboolean bar_shadow_visible,
      jfloat bar_shadow_opacity);

  void SetContentTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jobject>& jcontent_tree);

  void HideTree(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj);

 private:
  float base_page_brightness_;

  scoped_refptr<cc::Layer> content_layer_;
  scoped_refptr<ReaderModeLayer> reader_mode_layer_;
  scoped_refptr<cc::SolidColorLayer> color_overlay_;
  scoped_refptr<cc::Layer> content_container_;

  DISALLOW_COPY_AND_ASSIGN(ReaderModeSceneLayer);
};

bool RegisterReaderModeSceneLayer(JNIEnv* env);

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_READER_MODE_SCENE_LAYER_H_
