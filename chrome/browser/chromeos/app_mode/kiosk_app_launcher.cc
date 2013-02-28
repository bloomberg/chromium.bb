// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_launcher.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/chrome_restart_request.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

// static
KioskAppLauncher* KioskAppLauncher::running_instance_ = NULL;

KioskAppLauncher::KioskAppLauncher(const std::string& app_id,
                                   const LaunchCallback& callback)
    : app_id_(app_id),
      callback_(callback),
      success_(false) {
}

KioskAppLauncher::~KioskAppLauncher() {}

void KioskAppLauncher::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (running_instance_) {
    LOG(WARNING) << "Unable to launch " << app_id_ << "with a pending launch.";
    ReportLaunchResult(false);
    return;
  }

  running_instance_ = this;  // Reset in ReportLaunchResult.

  StartMount();  // Flow continues in MountCallback.
}

void KioskAppLauncher::ReportLaunchResult(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  running_instance_ = NULL;
  success_ = success;

  if (!callback_.is_null())
    callback_.Run(success_);
}

void KioskAppLauncher::StartMount() {
  const std::string token =
      CrosLibrary::Get()->GetCertLibrary()->EncryptToken(app_id_);

  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMount(
      app_id_,
      token,
      cryptohome::CREATE_IF_MISSING,
      base::Bind(&KioskAppLauncher::MountCallback,
                 base::Unretained(this)));
}

void KioskAppLauncher::MountCallback(bool mount_success,
                                     cryptohome::MountError mount_error) {
  if (!mount_success) {
    LOG(ERROR) << "Failed to mount, error=" << mount_error;
    ReportLaunchResult(false);
    return;
  }

  RestartChrome(GetKioskAppCommandLine(app_id_));
  ReportLaunchResult(true);
}

}  // namespace chromeos
