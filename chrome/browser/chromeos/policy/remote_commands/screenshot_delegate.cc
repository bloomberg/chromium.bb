// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/screenshot_delegate.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/upload_job_impl.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "content/public/browser/browser_thread.h"

namespace policy {

ScreenshotDelegate::ScreenshotDelegate() : weak_ptr_factory_(this) {}

ScreenshotDelegate::~ScreenshotDelegate() {
}

bool ScreenshotDelegate::IsScreenshotAllowed() {
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceCloudPolicyManagerChromeOS* manager =
      connector->GetDeviceCloudPolicyManager();
  // DeviceCloudPolicyManagerChromeOS and StatusUploader can be null during
  // shutdown (and unit tests) - don't allow screenshots unless we have a
  // StatusUploader that can confirm that screenshots are allowed.
  return manager && manager->GetStatusUploader() &&
         manager->GetStatusUploader()->IsSessionDataUploadAllowed();
}

void ScreenshotDelegate::TakeSnapshot(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    const ui::GrabWindowSnapshotAsyncPNGCallback& callback) {
  ui::GrabWindowSnapshotAsyncPNG(
      window, source_rect,
      base::Bind(&ScreenshotDelegate::StoreScreenshot,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

std::unique_ptr<UploadJob> ScreenshotDelegate::CreateUploadJob(
    const GURL& upload_url,
    UploadJob::Delegate* delegate) {
  chromeos::DeviceOAuth2TokenService* device_oauth2_token_service =
      chromeos::DeviceOAuth2TokenServiceFactory::Get();

  scoped_refptr<net::URLRequestContextGetter> system_request_context =
      g_browser_process->system_request_context();
  std::string robot_account_id =
      device_oauth2_token_service->GetRobotAccountId();

  SYSLOG(INFO) << "Creating upload job for screenshot";
  return std::unique_ptr<UploadJob>(new UploadJobImpl(
      upload_url, robot_account_id, device_oauth2_token_service,
      system_request_context, delegate,
      base::WrapUnique(new UploadJobImpl::RandomMimeBoundaryGenerator),
      base::ThreadTaskRunnerHandle::Get()));
}

void ScreenshotDelegate::StoreScreenshot(
    const ui::GrabWindowSnapshotAsyncPNGCallback& callback,
    scoped_refptr<base::RefCountedMemory> png_data) {
  callback.Run(png_data);
}

}  // namespace policy
