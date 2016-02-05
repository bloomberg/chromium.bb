// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/reader_mode_scene_layer.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "chrome/browser/android/compositor/layer/reader_mode_layer.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/ReaderModeSceneLayer_jni.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/gfx/android/java_bitmap.h"

namespace chrome {
namespace android {

ReaderModeSceneLayer::ReaderModeSceneLayer(JNIEnv* env, jobject jobj)
    : SceneLayer(env, jobj) {
}

ReaderModeSceneLayer::~ReaderModeSceneLayer() {
}

void ReaderModeSceneLayer::CreateReaderModeLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jobject>& jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  reader_mode_layer_ = ReaderModeLayer::Create(resource_manager);
  layer_->AddChild(reader_mode_layer_->layer());
}

void ReaderModeSceneLayer::SetResourceIds(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    jint text_resource_id,
    jint bar_background_resource_id,
    jint bar_shadow_resource_id,
    jint panel_icon_resource_id,
    jint close_icon_resource_id) {

  reader_mode_layer_->SetResourceIds(
      text_resource_id,
      bar_background_resource_id,
      bar_shadow_resource_id,
      panel_icon_resource_id,
      close_icon_resource_id);
}

void ReaderModeSceneLayer::Update(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    jfloat dp_to_px,
    const JavaParamRef<jobject>& jcontent_view_core,
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
    jfloat bar_shadow_opacity) {
  // NOTE(mdjones): It is possible to render the panel before content has been
  // created. If this is the case, do not attempt to access the ContentViewCore
  // and instead pass null.
  content::ContentViewCore* content_view_core =
      !jcontent_view_core ? NULL
                          : content::ContentViewCore::GetNativeContentViewCore(
                                env, jcontent_view_core);

  reader_mode_layer_->SetProperties(
      dp_to_px,
      content_view_core,
      panel_X,
      panel_y,
      panel_width,
      panel_height,
      bar_margin_side,
      bar_height,
      text_opacity,
      bar_border_visible,
      bar_border_height,
      bar_shadow_visible,
      bar_shadow_opacity);
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  ReaderModeSceneLayer* reader_mode_scene_layer =
      new ReaderModeSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(reader_mode_scene_layer);
}

bool RegisterReaderModeSceneLayer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
