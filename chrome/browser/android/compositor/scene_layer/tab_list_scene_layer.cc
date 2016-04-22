// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/tab_list_scene_layer.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "chrome/browser/android/compositor/layer/tab_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "content/public/browser/android/compositor.h"
#include "jni/TabListSceneLayer_jni.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace chrome {
namespace android {

TabListSceneLayer::TabListSceneLayer(JNIEnv* env, jobject jobj)
    : SceneLayer(env, jobj),
      content_obscures_self_(false),
      write_index_(0),
      resource_manager_(nullptr),
      layer_title_cache_(nullptr),
      tab_content_manager_(nullptr),
      background_color_(SK_ColorWHITE),
      own_tree_(cc::Layer::Create()) {
  layer()->AddChild(own_tree_);
}

TabListSceneLayer::~TabListSceneLayer() {
}

void TabListSceneLayer::SetContentTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (content_tree && content_tree->layer()) {
    content_tree_ = content_tree->layer();
  } else {
    content_tree_->RemoveFromParent();
    content_tree_ = nullptr;
  }

  if (content_tree_.get())
    layer()->AddChild(content_tree_);
}

void TabListSceneLayer::BeginBuildingFrame(JNIEnv* env,
                                           const JavaParamRef<jobject>& jobj) {
  write_index_ = 0;
  content_obscures_self_ = false;
}

void TabListSceneLayer::FinishBuildingFrame(JNIEnv* env,
                                            const JavaParamRef<jobject>& jobj) {
  if (layers_.size() > write_index_)
    RemoveTabLayersInRange(write_index_, layers_.size());
}

void TabListSceneLayer::UpdateLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jint background_color,
    jfloat viewport_x,
    jfloat viewport_y,
    jfloat viewport_width,
    jfloat viewport_height,
    const JavaParamRef<jobject>& jlayer_title_cache,
    const JavaParamRef<jobject>& jtab_content_manager,
    const JavaParamRef<jobject>& jresource_manager) {
  // TODO(changwan): move these to constructor if possible
  if (resource_manager_ == nullptr) {
    resource_manager_ =
        ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  }
  if (layer_title_cache_ == nullptr)
    layer_title_cache_ = LayerTitleCache::FromJavaObject(jlayer_title_cache);
  if (tab_content_manager_ == nullptr) {
    tab_content_manager_ =
        TabContentManager::FromJavaObject(jtab_content_manager);
  }

  background_color_ = background_color;
  own_tree_->SetPosition(gfx::PointF(viewport_x, viewport_y));
  own_tree_->SetBounds(gfx::Size(viewport_width, viewport_height));
}

void TabListSceneLayer::PutTabLayer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
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
    jboolean inset_border) {
  scoped_refptr<TabLayer> layer = GetNextLayer(incognito);
  // https://crbug.com/517314: GetNextLayer() returns null in some corner cases.
  DCHECK(layer);
  if (layer) {
    layer->SetProperties(
        id, can_use_live_layer, toolbar_resource_id, close_button_resource_id,
        shadow_resource_id, contour_resource_id, back_logo_resource_id,
        border_resource_id, border_inner_shadow_resource_id,
        tab_background_color, back_logo_color, is_portrait, x, y, width, height,
        shadow_x, shadow_y, shadow_width, shadow_height, pivot_x, pivot_y,
        rotation_x, rotation_y, alpha, border_alpha, border_inner_shadow_alpha,
        contour_alpha, shadow_alpha, close_alpha, border_scale, saturation,
        brightness, close_btn_width, static_to_view_blend, content_width,
        content_height, content_width, visible_content_height, show_toolbar,
        toolbar_background_color, anonymize_toolbar,
        toolbar_textbox_resource_id, toolbar_textbox_background_color,
        toolbar_textbox_alpha, toolbar_alpha, toolbar_y_offset,
        side_border_scale, attach_content, inset_border);
  }

  if (attach_content) {
    gfx::RectF self(own_tree_->position(), gfx::SizeF(own_tree_->bounds()));
    gfx::RectF content(x, y, width, height);

    content_obscures_self_ |= content.Contains(self);
  }
}

base::android::ScopedJavaLocalRef<jobject> TabListSceneLayer::GetJavaObject(
    JNIEnv* env) {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void TabListSceneLayer::OnDetach() {
  SceneLayer::OnDetach();
  RemoveAllRemainingTabLayers();
}

bool TabListSceneLayer::ShouldShowBackground() {
  return !content_obscures_self_;
}

SkColor TabListSceneLayer::GetBackgroundColor() {
  return background_color_;
}

void TabListSceneLayer::RemoveAllRemainingTabLayers() {
  if (layers_.size() > 0)
    RemoveTabLayersInRange(0, layers_.size());
}

void TabListSceneLayer::RemoveTabLayersInRange(unsigned start, unsigned end) {
  DCHECK_LT(start, end);
  DCHECK_LE(end, layers_.size());
  DCHECK_LE(0u, start);
  for (unsigned i = start; i < end; ++i)
    layers_[i]->layer()->RemoveFromParent();
  layers_.erase(layers_.begin() + start, layers_.begin() + end);
}

scoped_refptr<TabLayer> TabListSceneLayer::GetNextLayer(bool incognito) {
  while (write_index_ < layers_.size()) {
    scoped_refptr<TabLayer> potential = layers_[write_index_];
    if (potential->is_incognito() == incognito)
      break;
    potential->layer()->RemoveFromParent();
    layers_.erase(layers_.begin() + write_index_);
  }

  if (write_index_ < layers_.size())
    return layers_[write_index_++];

  scoped_refptr<TabLayer> layer = TabLayer::Create(
      incognito, resource_manager_, layer_title_cache_, tab_content_manager_);
  layers_.push_back(layer);
  own_tree_->AddChild(layer->layer());
  write_index_++;
  return layer;
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  TabListSceneLayer* scene_layer = new TabListSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

bool RegisterTabListSceneLayer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
