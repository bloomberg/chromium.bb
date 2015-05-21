// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_READER_MODE_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_READER_MODE_SCENE_LAYER_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"

namespace chrome {
namespace android {
class ReaderModeLayer;

class ReaderModeSceneLayer : public SceneLayer {
 public:
  ReaderModeSceneLayer(JNIEnv* env, jobject jobj);
  ~ReaderModeSceneLayer() override;

  void UpdateReaderModeLayer(JNIEnv* env,
                             jobject object,
                             jint panel_background_resource_id,
                             jint panel_text_resource_id,
                             jobject jreader_mode_content_view_core,
                             jfloat panel_y,
                             jfloat panel_width,
                             jfloat panel_margin_top,
                             jfloat panel_height,
                             jfloat distilled_y,
                             jfloat distilled_height,
                             jfloat x,
                             jfloat panel_text_opacity,
                             jint header_background_color,
                             jobject jresource_manager);

 private:
  scoped_refptr<ReaderModeLayer> reader_mode_layer_;

  DISALLOW_COPY_AND_ASSIGN(ReaderModeSceneLayer);
};

bool RegisterReaderModeSceneLayer(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_READER_MODE_SCENE_LAYER_H_
