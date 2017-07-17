// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_CONTENT_MANAGER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_CONTENT_MANAGER_H_

#include <jni.h>

#include <map>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/android/thumbnail/thumbnail_cache.h"

using base::android::ScopedJavaLocalRef;

namespace cc {
class Layer;
}

namespace ui {
class UIResourceProvider;
}

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

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void SetUIResourceProvider(ui::UIResourceProvider* ui_resource_provider);

  // Get the live layer from the cache.
  scoped_refptr<cc::Layer> GetLiveLayer(int tab_id);

  scoped_refptr<ThumbnailLayer> GetStaticLayer(int tab_id);

  // Get the static thumbnail from the cache, or the NTP.
  scoped_refptr<ThumbnailLayer> GetOrCreateStaticLayer(int tab_id,
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
  jboolean HasFullCachedThumbnail(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint tab_id);
  void CacheTab(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                const base::android::JavaParamRef<jobject>& tab,
                jfloat thumbnail_scale);
  void CacheTabWithBitmap(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          const base::android::JavaParamRef<jobject>& tab,
                          const base::android::JavaParamRef<jobject>& bitmap,
                          jfloat thumbnail_scale);
  void InvalidateIfChanged(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jint tab_id,
                           const base::android::JavaParamRef<jstring>& jurl);
  void UpdateVisibleIds(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jintArray>& priority,
                        jint primary_tab_id);
  void NativeRemoveTabThumbnail(int tab_id);
  void RemoveTabThumbnail(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint tab_id);
  void GetDecompressedThumbnail(JNIEnv* env,
                                const base::android::JavaParamRef<jobject>& obj,
                                jint tab_id);
  void OnUIResourcesWereEvicted();

  // ThumbnailCacheObserver implementation;
  void OnFinishedThumbnailRead(TabId tab_id) override;

 private:
  class TabReadbackRequest;
  // TODO(bug 714384) check sizes and consider using base::flat_map if these
  // layer maps are small.
  using LayerMap = std::map<int, scoped_refptr<cc::Layer>>;
  using ThumbnailLayerMap = std::map<int, scoped_refptr<ThumbnailLayer>>;
  using TabReadbackRequestMap =
      base::flat_map<int, std::unique_ptr<TabReadbackRequest>>;

  void PutThumbnailIntoCache(int tab_id,
                             float thumbnail_scale,
                             const SkBitmap& bitmap);

  std::unique_ptr<ThumbnailCache> thumbnail_cache_;
  ThumbnailLayerMap static_layer_cache_;
  LayerMap live_layer_list_;
  TabReadbackRequestMap pending_tab_readbacks_;

  JavaObjectWeakGlobalRef weak_java_tab_content_manager_;
  base::WeakPtrFactory<TabContentManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabContentManager);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_CONTENT_MANAGER_H_
