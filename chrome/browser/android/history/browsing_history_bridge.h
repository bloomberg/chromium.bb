// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_BROWSING_HISTORY_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_HISTORY_BROWSING_HISTORY_BRIDGE_H_

#include <jni.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/history/browsing_history_service_handler.h"

using base::android::JavaParamRef;

// The bridge for fetching browsing history information for the Android
// history UI. This queries the BrowsingHistoryService and listens
// for callbacks.
class BrowsingHistoryBridge : public BrowsingHistoryServiceHandler {

 public:
  explicit BrowsingHistoryBridge(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 jobject j_profile);
  void Destroy(JNIEnv*, const JavaParamRef<jobject>&);

  void QueryHistory(JNIEnv* env,
                    const JavaParamRef<jobject>& obj,
                    const JavaParamRef<jobject>& j_result_obj,
                    jstring j_query,
                    int64_t j_query_end_time);

  // Adds a HistoryEntry with the |j_url| and |j_timestamps| to the list of
  // items being removed. The removal will not be committed until
  // ::removeItems() is called.
  void MarkItemForRemoval(
      JNIEnv* env,
      const JavaParamRef<jobject>& obj,
      jstring j_url,
      const JavaParamRef<jlongArray>& j_timestamps);

  // Removes all items that have been marked for removal through
  // ::markItemForRemoval().
  void RemoveItems(JNIEnv* env,
                   const JavaParamRef<jobject>& obj);

  // BrowsingHistoryServiceHandler implementation
  void OnQueryComplete(
      std::vector<BrowsingHistoryService::HistoryEntry>* results,
      BrowsingHistoryService::QueryResultsInfo* query_results_info) override;
  void OnRemoveVisitsComplete() override;
  void OnRemoveVisitsFailed() override;
  void HistoryDeleted() override;
  void HasOtherFormsOfBrowsingHistory(
      bool has_other_forms, bool has_synced_results) override;

 private:
  ~BrowsingHistoryBridge() override;

  std::unique_ptr<BrowsingHistoryService> browsing_history_service_;
  base::android::ScopedJavaGlobalRef<jobject> j_history_service_obj_;
  base::android::ScopedJavaGlobalRef<jobject> j_query_result_obj_;

  std::vector<std::unique_ptr<BrowsingHistoryService::HistoryEntry>>
      items_to_remove_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryBridge);
};

bool RegisterBrowsingHistoryBridge(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_HISTORY_BROWSING_HISTORY_BRIDGE_H_
