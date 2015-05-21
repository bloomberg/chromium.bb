// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/reader_mode_scene_layer.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/compositor/layer/reader_mode_layer.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/ReaderModeSceneLayer_jni.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace chrome {
namespace android {

ReaderModeSceneLayer::ReaderModeSceneLayer(JNIEnv* env, jobject jobj)
    : SceneLayer(env, jobj) {
}

ReaderModeSceneLayer::~ReaderModeSceneLayer() {
}

void ReaderModeSceneLayer::UpdateReaderModeLayer(
    JNIEnv* env,
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
    jobject jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  // Lazily construct the reader mode layer, as the feature is only
  // conditionally enabled.
  if (!reader_mode_layer_.get()) {
    if (!resource_manager)
      return;
    reader_mode_layer_ = ReaderModeLayer::Create(resource_manager);
    layer_->AddChild(reader_mode_layer_->layer());
  }

  // NOTE(pedrosimonetti): The ContentViewCore might not exist at this time if
  // the Reader Mode has not been requested yet. In this case,
  // we'll pass NULL to Reader Mode's Layer Tree.
  content::ContentViewCore* content_view_core =
      !jreader_mode_content_view_core ? NULL
                          : content::ContentViewCore::GetNativeContentViewCore(
                                env, jreader_mode_content_view_core);

  reader_mode_layer_->SetProperties(
      panel_background_resource_id, panel_text_resource_id,
      content_view_core,
      panel_y, panel_width, panel_margin_top, panel_height,
      distilled_y, distilled_height,
      x,
      panel_text_opacity, header_background_color);
}

static jlong Init(JNIEnv* env, jobject jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  ReaderModeSceneLayer* tree_provider =
      new ReaderModeSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(tree_provider);
}

bool RegisterReaderModeSceneLayer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
