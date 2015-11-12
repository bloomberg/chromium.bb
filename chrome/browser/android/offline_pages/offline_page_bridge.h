// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "components/offline_pages/offline_page_model.h"

namespace content {
class BrowserContext;
}

namespace offline_pages {
namespace android {

/**
 * Bridge between C++ and Java for exposing native implementation of offline
 * pages model in managed code.
 */
class OfflinePageBridge : public OfflinePageModel::Observer {
 public:
  // Returns true if |url| might points to an offline page.
  static bool MightBeOfflineURL(const GURL& url);

  OfflinePageBridge(JNIEnv* env,
                    jobject obj,
                    content::BrowserContext* browser_context);
  void Destroy(JNIEnv*, jobject);

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageModelChanged(OfflinePageModel* model) override;
  void OfflinePageDeleted(int64 bookmark_id) override;

  void GetAllPages(JNIEnv* env,
                   jobject obj,
                   jobject j_result_obj);
  void GetPagesToCleanUp(JNIEnv* env, jobject obj, jobject j_result_obj);

  base::android::ScopedJavaLocalRef<jobject>
  GetPageByBookmarkId(JNIEnv* env, jobject obj, jlong bookmark_id);

  void SavePage(JNIEnv* env,
                jobject obj,
                jobject j_callback_obj,
                jobject j_web_contents,
                jlong bookmark_id);

  void MarkPageAccessed(JNIEnv* env,
                        jobject obj,
                        jlong bookmark_id);

  void DeletePage(JNIEnv* env,
                  jobject obj,
                  jobject j_callback_obj,
                  jlong bookmark_id);

  void DeletePages(JNIEnv* env,
                   jobject obj,
                   jobject j_callback_obj,
                   jlongArray bookmark_ids_array);

  void CheckMetadataConsistency(JNIEnv* env, jobject obj);

 private:
  void NotifyIfDoneLoading() const;

  JavaObjectWeakGlobalRef weak_java_ref_;
  // Not owned.
  OfflinePageModel* offline_page_model_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageBridge);
};

bool RegisterOfflinePageBridge(JNIEnv* env);

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_BRIDGE_H_

