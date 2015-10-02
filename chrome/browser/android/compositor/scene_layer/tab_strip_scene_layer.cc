// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/scene_layer/tab_strip_scene_layer.h"

#include "base/android/jni_android.h"
#include "cc/resources/scoped_ui_resource.h"
#include "chrome/browser/android/compositor/layer/tab_handle_layer.h"
#include "chrome/browser/android/compositor/layer_title_cache.h"
#include "content/public/browser/android/compositor.h"
#include "jni/TabStripSceneLayer_jni.h"
#include "ui/android/resources/resource_manager_impl.h"

namespace chrome {
namespace android {

TabStripSceneLayer::TabStripSceneLayer(JNIEnv* env, jobject jobj)
    : SceneLayer(env, jobj),
      tab_strip_layer_(
          cc::SolidColorLayer::Create(content::Compositor::LayerSettings())),
      new_tab_button_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      model_selector_button_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())),
      background_tab_brightness_(1.f),
      brightness_(1.f),
      write_index_(0),
      content_tree_(nullptr) {
  new_tab_button_->SetIsDrawable(true);
  model_selector_button_->SetIsDrawable(true);
  tab_strip_layer_->SetBackgroundColor(SK_ColorBLACK);
  tab_strip_layer_->SetIsDrawable(true);
  tab_strip_layer_->AddChild(new_tab_button_);
  tab_strip_layer_->AddChild(model_selector_button_);
  layer()->AddChild(tab_strip_layer_);
}

TabStripSceneLayer::~TabStripSceneLayer() {
}

void TabStripSceneLayer::SetContentTree(JNIEnv* env,
                                        jobject jobj,
                                        jobject jcontent_tree) {
  SceneLayer* content_tree = FromJavaObject(env, jcontent_tree);
  if (content_tree_ &&
      (!content_tree_->layer()->parent() ||
       content_tree_->layer()->parent()->id() != layer()->id()))
    content_tree_ = nullptr;

  if (content_tree != content_tree_) {
    if (content_tree_)
      content_tree_->layer()->RemoveFromParent();
    content_tree_ = content_tree;
    if (content_tree) {
      layer()->InsertChild(content_tree->layer(), 0);
      content_tree->layer()->SetPosition(
          gfx::PointF(0, -layer()->position().y()));
    }
  }
}

void TabStripSceneLayer::BeginBuildingFrame(JNIEnv* env,
                                            jobject jobj,
                                            jboolean visible) {
  write_index_ = 0;
  tab_strip_layer_->SetHideLayerAndSubtree(!visible);
}

void TabStripSceneLayer::FinishBuildingFrame(JNIEnv* env, jobject jobj) {
  if (tab_strip_layer_->hide_layer_and_subtree())
    return;

  for (unsigned i = write_index_; i < tab_handle_layers_.size(); ++i)
    tab_handle_layers_[i]->layer()->RemoveFromParent();

  tab_handle_layers_.erase(tab_handle_layers_.begin() + write_index_,
                           tab_handle_layers_.end());
}

void TabStripSceneLayer::UpdateTabStripLayer(JNIEnv* env,
                                             jobject jobj,
                                             jfloat width,
                                             jfloat height,
                                             jfloat y_offset,
                                             jfloat background_tab_brightness,
                                             jfloat brightness,
                                             jboolean should_readd_background) {
  background_tab_brightness_ = background_tab_brightness;
  gfx::RectF content(0, y_offset, width, height);
  layer()->SetPosition(gfx::PointF(0, y_offset));
  tab_strip_layer_->SetBounds(gfx::Size(width, height));

  if (brightness != brightness_) {
    brightness_ = brightness;
    cc::FilterOperations filters;
    if (brightness_ < 1.f)
      filters.Append(cc::FilterOperation::CreateBrightnessFilter(brightness_));
    tab_strip_layer_->SetFilters(filters);
  }

  // Content tree should not be affected by tab strip scene layer visibility.
  if (content_tree_)
    content_tree_->layer()->SetPosition(gfx::PointF(0, -y_offset));

  // Make sure tab strip changes are committed after rotating the device.
  // See https://crbug.com/503930 for more details.
  // InsertChild() forces the tree sync, which seems to fix the problem.
  // Note that this is a workaround.
  // TODO(changwan): find out why the update is not committed after rotation.
  if (should_readd_background) {
    int background_index = 0;
    if (content_tree_ && content_tree_->layer()) {
      background_index = 1;
    }
    DCHECK(layer()->children()[background_index] == tab_strip_layer_);
    layer()->InsertChild(tab_strip_layer_, background_index);
  }
}

void TabStripSceneLayer::UpdateNewTabButton(JNIEnv* env,
                                            jobject jobj,
                                            jint resource_id,
                                            jfloat x,
                                            jfloat y,
                                            jfloat width,
                                            jfloat height,
                                            jboolean visible,
                                            jobject jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::ResourceManager::Resource* button_resource =
      resource_manager->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                    resource_id);

  new_tab_button_->SetUIResourceId(button_resource->ui_resource->id());
  float left_offset = (width - button_resource->size.width()) / 2;
  float top_offset = (height - button_resource->size.height()) / 2;
  new_tab_button_->SetPosition(gfx::PointF(x + left_offset, y + top_offset));
  new_tab_button_->SetBounds(button_resource->size);
  new_tab_button_->SetHideLayerAndSubtree(!visible);
}

void TabStripSceneLayer::UpdateModelSelectorButton(JNIEnv* env,
                                                   jobject jobj,
                                                   jint resource_id,
                                                   jfloat x,
                                                   jfloat y,
                                                   jfloat width,
                                                   jfloat height,
                                                   jboolean incognito,
                                                   jboolean visible,
                                                   jobject jresource_manager) {
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  ui::ResourceManager::Resource* button_resource =
      resource_manager->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                    resource_id);

  model_selector_button_->SetUIResourceId(button_resource->ui_resource->id());
  float left_offset = (width - button_resource->size.width()) / 2;
  float top_offset = (height - button_resource->size.height()) / 2;
  model_selector_button_->SetPosition(
      gfx::PointF(x + left_offset, y + top_offset));
  model_selector_button_->SetBounds(button_resource->size);
  model_selector_button_->SetHideLayerAndSubtree(!visible);
}

void TabStripSceneLayer::PutStripTabLayer(JNIEnv* env,
                                          jobject jobj,
                                          jint id,
                                          jint close_resource_id,
                                          jint handle_resource_id,
                                          jboolean foreground,
                                          jboolean close_pressed,
                                          jfloat toolbar_width,
                                          jfloat x,
                                          jfloat y,
                                          jfloat width,
                                          jfloat height,
                                          jfloat content_offset_x,
                                          jfloat close_button_alpha,
                                          jboolean is_loading,
                                          jfloat spinner_rotation,
                                          jfloat border_opacity,
                                          jobject jlayer_title_cache,
                                          jobject jresource_manager) {
  LayerTitleCache* layer_title_cache =
      LayerTitleCache::FromJavaObject(jlayer_title_cache);
  ui::ResourceManager* resource_manager =
      ui::ResourceManagerImpl::FromJavaObject(jresource_manager);
  scoped_refptr<TabHandleLayer> layer = GetNextLayer(layer_title_cache);
  ui::ResourceManager::Resource* tab_handle_resource =
      resource_manager->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                    handle_resource_id);
  ui::ResourceManager::Resource* close_button_resource =
      resource_manager->GetResource(ui::ANDROID_RESOURCE_TYPE_STATIC,
                                    close_resource_id);
  layer->SetProperties(id, close_button_resource, tab_handle_resource,
                       foreground, close_pressed, toolbar_width, x, y, width,
                       height, content_offset_x, close_button_alpha, is_loading,
                       spinner_rotation, background_tab_brightness_,
                       border_opacity);
}

scoped_refptr<TabHandleLayer> TabStripSceneLayer::GetNextLayer(
    LayerTitleCache* layer_title_cache) {
  if (write_index_ < tab_handle_layers_.size())
    return tab_handle_layers_[write_index_++];

  scoped_refptr<TabHandleLayer> layer_tree =
      TabHandleLayer::Create(layer_title_cache);
  tab_handle_layers_.push_back(layer_tree);
  tab_strip_layer_->AddChild(layer_tree->layer());
  write_index_++;
  return layer_tree;
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  // This will automatically bind to the Java object and pass ownership there.
  TabStripSceneLayer* scene_layer = new TabStripSceneLayer(env, jobj);
  return reinterpret_cast<intptr_t>(scene_layer);
}

bool RegisterTabStripSceneLayer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
