// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_EVALUATION_OFFLINE_PAGE_EVALUATION_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_EVALUATION_OFFLINE_PAGE_EVALUATION_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/offline_pages/background/request_coordinator.h"
#include "components/offline_pages/background/request_notifier.h"
#include "components/offline_pages/offline_page_model.h"

namespace content {
class BrowserContext;
}

namespace offline_pages {

namespace android {

/**
 * Bridge for exposing native implementation which are used by evaluation.
 */
class OfflinePageEvaluationBridge : public OfflinePageModel::Observer,
                                    public RequestCoordinator::Observer {
 public:
  static bool Register(JNIEnv* env);
  static std::unique_ptr<KeyedService> GetTestingRequestCoordinator(
      content::BrowserContext* context);

  OfflinePageEvaluationBridge(JNIEnv* env,
                              content::BrowserContext* browser_context,
                              OfflinePageModel* offline_page_model,
                              RequestCoordinator* request_coordinator);

  ~OfflinePageEvaluationBridge() override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageModelChanged(OfflinePageModel* model) override;
  void OfflinePageDeleted(int64_t offline_id,
                          const ClientId& client_id) override;

  // RequestCoordinator::Observer implementation.
  void OnAdded(const SavePageRequest& request) override;
  void OnCompleted(const SavePageRequest& request,
                   RequestNotifier::BackgroundSavePageResult status) override;
  void OnChanged(const SavePageRequest& request) override;

  void GetAllPages(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_result_obj,
                   const base::android::JavaParamRef<jobject>& j_callback_obj);

  // Return true if processing starts and callback is expected to be called.
  // False otherwise.
  bool PushRequestProcessing(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  // Gets all the requests in the queue.
  void GetRequestsInQueue(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  // Removes the requests from the queue.
  void RemoveRequestsFromQueue(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jlongArray>& j_request_ids,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

  void SavePageLater(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jstring>& url,
                     const base::android::JavaParamRef<jstring>& j_namespace,
                     const base::android::JavaParamRef<jstring>& j_client_id,
                     jboolean user_requested);

  base::android::ScopedJavaGlobalRef<jobject> java_ref() { return java_ref_; }

 private:
  void NotifyIfDoneLoading() const;

  base::android::ScopedJavaLocalRef<jobject> CreateClientId(
      JNIEnv* env,
      const ClientId& clientId) const;

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  // Not owned.
  content::BrowserContext* browser_context_;
  // Not owned.
  OfflinePageModel* offline_page_model_;
  // Not owned.
  RequestCoordinator* request_coordinator_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageEvaluationBridge);
};

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_EVALUATION_OFFLINE_PAGE_EVALUATION_BRIDGE_H_
