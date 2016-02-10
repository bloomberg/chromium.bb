// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MANAGER_SERVICE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MANAGER_SERVICE_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "content/public/browser/download_manager.h"

namespace content {
class DownloadItem;
}

// Native side of DownloadManagerService.java. The native object is owned by its
// Java object.
class DownloadManagerService : public content::DownloadManager::Observer {
 public:
  // JNI registration.
  static bool RegisterDownloadManagerService(JNIEnv* env);

  DownloadManagerService(JNIEnv* env,
                         jobject jobj,
                         content::DownloadManager* manager);
  ~DownloadManagerService() override;

  // Called to resume downloading the item that has ID equal to |download_id|.
  // If the DownloadItem is not yet created, retry after a while.
  void ResumeDownload(JNIEnv* env,
                      jobject obj,
                      uint32_t download_id,
                      jstring fileName);

  // Called to cancel a download item that has ID equal to |download_id|.
  // If the DownloadItem is not yet created, retry after a while.
  void CancelDownload(JNIEnv* env, jobject obj, uint32_t download_id);

  // Called to pause a download item that has ID equal to |download_id|.
  // If the DownloadItem is not yet created, do nothing as it is already paused.
  void PauseDownload(JNIEnv* env, jobject obj, uint32_t download_id);

  // content::DownloadManager::Observer methods.
  void ManagerGoingDown(content::DownloadManager* manager) override;

 private:
  // For testing.
  friend class DownloadManagerServiceTest;

  // Resume downloading the given DownloadItem.
  void ResumeDownloadItem(content::DownloadItem* item,
                          const std::string& fileName);

  // Helper function to start the download resumption. If |retry| is true,
  // chrome will retry the resumption if the download item is not loaded.
  void ResumeDownloadInternal(uint32_t download_id,
                              const std::string& fileName,
                              bool retry);

  // Helper function to cancel a download. If |retry| is true,
  // chrome will retry the cancellation if the download item is not loaded.
  void CancelDownloadInternal(uint32_t download_id, bool retry);

  // Called to notify the java side that download resumption failed.
  void OnResumptionFailed(uint32_t download_id, const std::string& fileName);

  typedef base::Callback<void(bool)> ResumeCallback;
  void set_resume_callback_for_testing(const ResumeCallback& resume_cb) {
    resume_callback_for_testing_ = resume_cb;
  }

  // Reference to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Download manager this class observes
  content::DownloadManager* manager_;

  ResumeCallback resume_callback_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerService);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MANAGER_SERVICE_H_
