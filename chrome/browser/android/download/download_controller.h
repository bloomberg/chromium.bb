// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class pairs with DownloadController on Java side to forward requests
// for GET downloads to the current DownloadListener. POST downloads are
// handled on the native side.
//
// Both classes are Singleton classes. C++ object owns Java object.
//
// Call sequence
// GET downloads:
// DownloadController::CreateGETDownload() =>
// DownloadController.newHttpGetDownload() =>
// DownloadListener.onDownloadStart() /
// DownloadListener2.requestHttpGetDownload()
//

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_CONTROLLER_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_CONTROLLER_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"
#include "chrome/browser/android/download/download_controller_base.h"

namespace ui {
class WindowAndroid;
}

namespace content {
class WebContents;
}

class DownloadController : public DownloadControllerBase {
 public:
  static DownloadController* GetInstance();

  static bool RegisterDownloadController(JNIEnv* env);

  // Called when DownloadController Java object is instantiated.
  void Init(JNIEnv* env, jobject obj);

  // DownloadControllerBase implementation.
  void AcquireFileAccessPermission(
      content::WebContents* web_contents,
      const AcquireFileAccessPermissionCallback& callback) override;

  // UMA histogram enum for download cancellation reasons. Keep this
  // in sync with MobileDownloadCancelReason in histograms.xml. This should be
  // append only.
  enum DownloadCancelReason {
    CANCEL_REASON_NOT_CANCELED = 0,
    CANCEL_REASON_ACTION_BUTTON,
    CANCEL_REASON_NOTIFICATION_DISMISSED,
    CANCEL_REASON_OVERWRITE_INFOBAR_DISMISSED,
    CANCEL_REASON_NO_STORAGE_PERMISSION,
    CANCEL_REASON_DANGEROUS_DOWNLOAD_INFOBAR_DISMISSED,
    CANCEL_REASON_NO_EXTERNAL_STORAGE,
    CANCEL_REASON_CANNOT_DETERMINE_DOWNLOAD_TARGET,
    CANCEL_REASON_OTHER_NATIVE_RESONS,
    CANCEL_REASON_MAX
  };
  static void RecordDownloadCancelReason(DownloadCancelReason reason);

 private:
  struct JavaObject;
  friend struct base::DefaultSingletonTraits<DownloadController>;
  DownloadController();
  ~DownloadController() override;

  // Helper method for implementing AcquireFileAccessPermission().
  bool HasFileAccessPermission(ui::WindowAndroid* window_android);

  // DownloadControllerBase implementation.
  void OnDownloadStarted(content::DownloadItem* download_item) override;
  void StartContextMenuDownload(const content::ContextMenuParams& params,
                                content::WebContents* web_contents,
                                bool is_link,
                                const std::string& extra_headers) override;

  // DownloadItem::Observer interface.
  void OnDownloadUpdated(content::DownloadItem* item) override;

  // The download item contains dangerous file types.
  void OnDangerousDownload(content::DownloadItem *item);

  base::android::ScopedJavaLocalRef<jobject> GetContentViewCoreFromWebContents(
      content::WebContents* web_contents);

  // Creates Java object if it is not created already and returns it.
  JavaObject* GetJavaObject();

  JavaObject* java_object_;

  std::string default_file_name_;

  DISALLOW_COPY_AND_ASSIGN(DownloadController);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_CONTROLLER_H_
