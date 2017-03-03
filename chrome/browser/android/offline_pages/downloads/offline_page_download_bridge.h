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
#include "components/offline_pages/core/downloads/download_ui_adapter.h"

namespace content {
class BrowserContext;
}

namespace offline_pages {
namespace android {

/**
 * Bridge between C++ and Java for exposing native implementation of offline
 * pages model in managed code.
 */
class OfflinePageDownloadBridge : public DownloadUIAdapter::Observer {
 public:
  OfflinePageDownloadBridge(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            DownloadUIAdapter* download_ui_adapter,
                            content::BrowserContext* browser_context);
  ~OfflinePageDownloadBridge() override;

  static bool Register(JNIEnv* env);

  void Destroy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void GetAllItems(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_result_obj);

  base::android::ScopedJavaLocalRef<jobject> GetItemByGuid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_guid);

  void DeleteItemByGuid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_guid);

  jlong GetOfflineIdByGuid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_guid);

  void StartDownload(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_tab);

  void CancelDownload(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      const base::android::JavaParamRef<jstring>& j_guid);

  void PauseDownload(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jstring>& j_guid);

  void ResumeDownload(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      const base::android::JavaParamRef<jstring>& j_guid);

  void ResumePendingRequestImmediately(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // DownloadUIAdapter::Observer implementation.
  void ItemsLoaded() override;
  void ItemAdded(const DownloadUIItem& item) override;
  void ItemUpdated(const DownloadUIItem& item) override;
  void ItemDeleted(const std::string& guid) override;

 private:
  JavaObjectWeakGlobalRef weak_java_ref_;
  // Not owned.
  DownloadUIAdapter* download_ui_adapter_;
  // Not owned.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageDownloadBridge);
};

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_DOWNLOAD_BRIDGE_H_
