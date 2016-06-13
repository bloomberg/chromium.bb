// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MANAGER_SERVICE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MANAGER_SERVICE_H_

#include <jni.h>
#include <map>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/download/download_history.h"
#include "content/public/browser/download_manager.h"

using base::android::JavaParamRef;

namespace content {
class DownloadItem;
}

// Native side of DownloadManagerService.java. The native object is owned by its
// Java object.
class DownloadManagerService : public DownloadHistory::Observer {
 public:
  // JNI registration.
  static bool RegisterDownloadManagerService(JNIEnv* env);

  DownloadManagerService(JNIEnv* env,
                         jobject jobj);
  ~DownloadManagerService() override;

  // Called to resume downloading the item that has GUID equal to
  // |jdownload_guid|..
  void ResumeDownload(JNIEnv* env,
                      jobject obj,
                      const JavaParamRef<jstring>& jdownload_guid);

  // Called to cancel a download item that has GUID equal to |jdownload_guid|.
  // If the DownloadItem is not yet created, retry after a while.
  void CancelDownload(JNIEnv* env,
                      jobject obj,
                      const JavaParamRef<jstring>& jdownload_guid,
                      bool is_off_the_record,
                      bool is_notification_dismissed);

  // Called to pause a download item that has GUID equal to |jdownload_guid|.
  // If the DownloadItem is not yet created, do nothing as it is already paused.
  void PauseDownload(JNIEnv* env,
                     jobject obj,
                     const JavaParamRef<jstring>& jdownload_guid);

  // DownloadHistory::Observer methods.
  void OnHistoryQueryComplete() override;

 protected:
  // Called to get the content::DownloadManager instance.
  virtual content::DownloadManager* GetDownloadManager(bool is_off_the_record);

 private:
  // For testing.
  friend class DownloadManagerServiceTest;

  // Helper function to start the download resumption.
  void ResumeDownloadInternal(const std::string& download_guid);

  // Helper function to cancel a download.
  void CancelDownloadInternal(const std::string& download_guid,
                              bool is_off_the_record);

  // Helper function to pause a download.
  void PauseDownloadInternal(const std::string& download_guid);

  // Called to notify the java side that download resumption failed.
  void OnResumptionFailed(const std::string& download_guid);

  void OnResumptionFailedInternal(const std::string& download_guid);

  typedef base::Callback<void(bool)> ResumeCallback;
  void set_resume_callback_for_testing(const ResumeCallback& resume_cb) {
    resume_callback_for_testing_ = resume_cb;
  }

  // Reference to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  bool is_history_query_complete_;

  enum DownloadAction { RESUME, PAUSE, CANCEL, UNKNOWN };

  using PendingDownloadActions = std::map<std::string, DownloadAction>;
  PendingDownloadActions pending_actions_;

  void EnqueueDownloadAction(const std::string& download_guid,
                             DownloadAction action);

  ResumeCallback resume_callback_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerService);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_MANAGER_SERVICE_H_
