// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_DOWNLOAD_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_DOWNLOAD_BRIDGE_H_

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/offline_pages/offline_page_item.h"
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
class OfflinePageDownloadBridge {
 public:
  OfflinePageDownloadBridge(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            content::BrowserContext* browser_context);
  ~OfflinePageDownloadBridge();

  static bool Register(JNIEnv* env);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void GetAllItems(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_result_obj);

  base::android::ScopedJavaLocalRef<jobject> GetItemByGuid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_guid);

  void OnOfflinePageDownloadBridgeLoaded();
  void OnOfflinePageDownloadItemAdded(const OfflinePageItem& item);
  void OnOfflinePageDownloadItemDeleted(const std::string& guid);
  void OnOfflinePageDownloadItemUpdated(const OfflinePageItem& item);

 private:
  JavaObjectWeakGlobalRef weak_java_ref_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageDownloadBridge);
};

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_DOWNLOAD_BRIDGE_H_
