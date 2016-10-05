// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/reader_mode_scene_layer.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "cc/layers/solid_color_layer.h"
#include "chrome/browser/android/compositor/layer/reader_mode_layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/web_contents.h"
#include "jni/ReaderModeSceneLayer_jni.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;

namespace android {

ReaderModeSceneLayer::ReaderModeSceneLayer(JNIEnv* env, jobject jobj)
    : SceneLayer(env, jobj),
      base_page_brightness_(1.0f),
      content_container_(cc::Layer::Create()) {
  // Responsible for moving the base page without modifying the layer itself.
  content_container_->SetIsDrawable(true);
  content_container_->SetPosition(gfx::PointF(0.0f, 0.0f));
  layer()->AddChild(content_container_);
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

  // The Reader Mode layer is initially invisible.
  reader_mode_layer_->layer()->SetHideLayerAndSubtree(true);

  layer()->AddChild(reader_mode_layer_->layer());
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
    jfloat base_page_brightness,
    jfloat base_page_offset,
    const JavaParamRef<jobject>& jweb_contents,
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
  // created. If this is the case, do not attempt to access the WebContents
  // and instead pass null.
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  scoped_refptr<cc::Layer> content_layer =
      web_contents ? web_contents->GetNativeView()->GetLayer() : nullptr;

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

  reader_mode_layer_->SetProperties(
      dp_to_px,
      content_layer,
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

  // Make the layer visible if it is not already.
  reader_mode_layer_->layer()->SetHideLayerAndSubtree(false);
}

void ReaderModeSceneLayer::SetContentTree(
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

void ReaderModeSceneLayer::HideTree(JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  // TODO(mdjones): Create super class for this logic.
  if (reader_mode_layer_) {
    reader_mode_layer_->layer()->SetHideLayerAndSubtree(true);
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
  ReaderModeSceneLayer* reader_mode_scene_layer =
      new ReaderModeSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(reader_mode_scene_layer);
}

bool RegisterReaderModeSceneLayer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
