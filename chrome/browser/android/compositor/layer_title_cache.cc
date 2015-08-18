// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer_title_cache.h"

#include <android/bitmap.h>

#include "base/memory/scoped_ptr.h"
#include "cc/layers/layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/android/compositor/decoration_title.h"
#include "jni/LayerTitleCache_jni.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace chrome {
namespace android {

// static
LayerTitleCache* LayerTitleCache::FromJavaObject(jobject jobj) {
  if (!jobj)
    return NULL;
  return reinterpret_cast<LayerTitleCache*>(Java_LayerTitleCache_getNativePtr(
      base::android::AttachCurrentThread(), jobj));
}

LayerTitleCache::LayerTitleCache(JNIEnv* env,
                                 jobject obj,
                                 jint fade_width,
                                 jint favicon_start_padding,
                                 jint favicon_end_padding,
                                 jint spinner_resource_id,
                                 jint spinner_incognito_resource_id)
    : weak_java_title_cache_(env, obj),
      fade_width_(fade_width),
      favicon_start_padding_(favicon_start_padding),
      favicon_end_padding_(favicon_end_padding),
      spinner_resource_id_(spinner_resource_id),
      spinner_incognito_resource_id_(spinner_incognito_resource_id),
      resource_manager_(nullptr) {
}

void LayerTitleCache::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void LayerTitleCache::UpdateLayer(JNIEnv* env,
                                  jobject obj,
                                  jint tab_id,
                                  jint title_resource_id,
                                  jint favicon_resource_id,
                                  bool is_incognito,
                                  bool is_rtl) {
  DecorationTitle* title_layer = layer_cache_.Lookup(tab_id);
  if (title_layer == NULL) {
    layer_cache_.AddWithID(
        new DecorationTitle(
            this, resource_manager_, title_resource_id, favicon_resource_id,
            spinner_resource_id_, spinner_incognito_resource_id_, fade_width_,
            favicon_start_padding_, favicon_end_padding_, is_incognito, is_rtl),
        tab_id);
  } else {
    if (title_resource_id != -1 && favicon_resource_id != -1) {
      title_layer->Update(title_resource_id, favicon_resource_id, fade_width_,
                          favicon_start_padding_, favicon_end_padding_,
                          is_incognito, is_rtl);
    } else {
      layer_cache_.Remove(tab_id);
    }
  }
}

void LayerTitleCache::ClearExcept(JNIEnv* env, jobject obj, jint except_id) {
  IDMap<DecorationTitle, IDMapOwnPointer>::iterator iter(&layer_cache_);
  for (; !iter.IsAtEnd(); iter.Advance()) {
    const int id = iter.GetCurrentKey();
    if (id != except_id)
      layer_cache_.Remove(id);
  }
}

DecorationTitle* LayerTitleCache::GetTitleLayer(int tab_id) {
  return layer_cache_.Lookup(tab_id);
}

void LayerTitleCache::SetResourceManager(
    ui::ResourceManager* resource_manager) {
  resource_manager_ = resource_manager;

  IDMap<DecorationTitle, IDMapOwnPointer>::iterator iter(&layer_cache_);
  for (; !iter.IsAtEnd(); iter.Advance()) {
    iter.GetCurrentValue()->SetResourceManager(resource_manager_);
  }
}

LayerTitleCache::~LayerTitleCache() {
}

bool RegisterLayerTitleCache(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env,
           jobject obj,
           jint fade_width,
           jint favicon_start_padding,
           jint favicon_end_padding,
           jint spinner_resource_id,
           jint spinner_incognito_resource_id) {
  LayerTitleCache* cache = new LayerTitleCache(
      env, obj, fade_width, favicon_start_padding, favicon_end_padding,
      spinner_resource_id, spinner_incognito_resource_id);
  return reinterpret_cast<intptr_t>(cache);
}

}  // namespace android
}  // namespace chrome
