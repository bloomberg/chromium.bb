// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_CONTENT_MANAGER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_CONTENT_MANAGER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/weak_ptr.h"
#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/android/thumbnail/thumbnail_cache.h"

using base::android::ScopedJavaLocalRef;

namespace cc {
class CopyOutputResult;
class Layer;
}

namespace ui {
class UIResourceProvider;
}

namespace chrome {
namespace android {

class ThumbnailLayer;

// A native component of the Java TabContentManager class.
class TabContentManager : public ThumbnailCacheObserver {
 public:
  static TabContentManager* FromJavaObject(jobject jobj);

  TabContentManager(JNIEnv* env,
                    jobject obj,
                    jint default_cache_size,
                    jint approximation_cache_size,
                    jint compression_queue_max_size,
                    jint write_queue_max_size,
                    jboolean use_approximation_thumbnail);

  virtual ~TabContentManager();

  void Destroy(JNIEnv* env, jobject obj);

  void SetUIResourceProvider(ui::UIResourceProvider* ui_resource_provider);

  // Get the live layer from the cache.
  scoped_refptr<cc::Layer> GetLiveLayer(int tab_id);

  // Get the static thumbnail from the cache, or the NTP.
  scoped_refptr<ThumbnailLayer> GetStaticLayer(int tab_id,
                                               bool force_disk_read);

  // Should be called when a tab gets a new live layer that should be served
  // by the cache to the CompositorView.
  void AttachLiveLayer(int tab_id, scoped_refptr<cc::Layer> layer);

  // Should be called when a tab removes a live layer because it should no
  // longer be served by the CompositorView.  If |layer| is NULL, will
  // make sure all live layers are detached.
  void DetachLiveLayer(int tab_id, scoped_refptr<cc::Layer> layer);

  // Callback for when the thumbnail decompression for tab_id is done.
  void OnFinishDecompressThumbnail(int tab_id, bool success, SkBitmap bitmap);
  // JNI methods.
  jboolean HasFullCachedThumbnail(JNIEnv* env, jobject obj, jint tab_id);
  void CacheTab(JNIEnv* env,
                jobject obj,
                jobject tab,
                jobject content_view_core,
                jfloat thumbnail_scale);
  void CacheTabWithBitmap(JNIEnv* env,
                          jobject obj,
                          jobject tab,
                          jobject bitmap,
                          jfloat thumbnail_scale);
  void InvalidateIfChanged(JNIEnv* env, jobject obj, jint tab_id, jstring jurl);
  void UpdateVisibleIds(JNIEnv* env, jobject obj, jintArray priority);
  void RemoveTabThumbnail(JNIEnv* env, jobject obj, jint tab_id);
  void RemoveTabThumbnailFromDiskAtAndAboveId(JNIEnv* env,
                                              jobject obj,
                                              jint min_forbidden_id);
  void GetDecompressedThumbnail(JNIEnv* env, jobject obj, jint tab_id);
  void OnUIResourcesWereEvicted();

  // ThumbnailCacheObserver implementation;
  void OnFinishedThumbnailRead(TabId tab_id) override;

 private:
  class TabReadbackRequest;
  typedef base::hash_map<int, scoped_refptr<cc::Layer>> LayerMap;
  typedef base::hash_map<int, scoped_refptr<ThumbnailLayer>> ThumbnailLayerMap;
  typedef base::ScopedPtrHashMap<int, scoped_ptr<TabReadbackRequest>>
      TabReadbackRequestMap;

  void PutThumbnailIntoCache(int tab_id,
                             float thumbnail_scale,
                             const SkBitmap& bitmap);

  scoped_ptr<ThumbnailCache> thumbnail_cache_;
  ThumbnailLayerMap static_layer_cache_;
  LayerMap live_layer_list_;
  TabReadbackRequestMap pending_tab_readbacks_;

  JavaObjectWeakGlobalRef weak_java_tab_content_manager_;
  base::WeakPtrFactory<TabContentManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabContentManager);
};

bool RegisterTabContentManager(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_CONTENT_MANAGER_H_
