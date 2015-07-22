// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/mock_download_controller_android.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"

using content::DownloadControllerAndroid;

namespace chrome {
namespace android {

MockDownloadControllerAndroid::MockDownloadControllerAndroid()
    : approve_file_access_request_(true) {
}

MockDownloadControllerAndroid::~MockDownloadControllerAndroid() {}

void MockDownloadControllerAndroid::CreateGETDownload(
    int render_process_id, int render_view_id, int request_id) {
}

void MockDownloadControllerAndroid::OnDownloadStarted(
    content::DownloadItem* download_item) {
}

void MockDownloadControllerAndroid::StartContextMenuDownload(
    const content::ContextMenuParams& params,
    content::WebContents* web_contents,
    bool is_link, const std::string& extra_headers) {
}

void MockDownloadControllerAndroid::DangerousDownloadValidated(
    content::WebContents* web_contents, int download_id, bool accept) {
}

void MockDownloadControllerAndroid::AcquireFileAccessPermission(
    int render_process_id,
    int render_view_id,
    const DownloadControllerAndroid::AcquireFileAccessPermissionCallback& cb) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(cb, approve_file_access_request_));
}

void MockDownloadControllerAndroid::SetApproveFileAccessRequestForTesting(
    bool approve) {
  approve_file_access_request_ = approve;
}

}  // namespace android
}  // namespace chrome
