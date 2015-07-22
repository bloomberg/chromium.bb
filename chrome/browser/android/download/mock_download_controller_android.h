// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_MOCK_DOWNLOAD_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_MOCK_DOWNLOAD_CONTROLLER_ANDROID_H_

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "content/public/browser/android/download_controller_android.h"

namespace content {
class DownloadItem;
class WebContents;
}  // namespace content

namespace chrome {
namespace android {

// Mock implementation of the DownloadControllerAndroid.
class MockDownloadControllerAndroid
    : public content::DownloadControllerAndroid {
 public:
  MockDownloadControllerAndroid();
  ~MockDownloadControllerAndroid() override;

  // DownloadControllerAndroid implementation.
  void CreateGETDownload(int render_process_id, int render_view_id,
                         int request_id) override;
  void OnDownloadStarted(content::DownloadItem* download_item) override;
  void StartContextMenuDownload(
      const content::ContextMenuParams& params,
      content::WebContents* web_contents,
      bool is_link, const std::string& extra_headers) override;
  void DangerousDownloadValidated(
      content::WebContents* web_contents, int download_id,
      bool accept) override;
  void AcquireFileAccessPermission(
      int render_process_id,
      int render_view_id,
      const AcquireFileAccessPermissionCallback& callback) override;
  void SetApproveFileAccessRequestForTesting(bool approve) override;

 private:
  bool approve_file_access_request_;
  DISALLOW_COPY_AND_ASSIGN(MockDownloadControllerAndroid);
};

}  // namespace android
}  // namespace chrome


#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_MOCK_DOWNLOAD_CONTROLLER_ANDROID_H_
