// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_launcher.h"

#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

namespace {

std::string GetAppUserNameFromAppId(const std::string& app_id) {
  return app_id + "@" + UserManager::kKioskAppUserDomain;
}

}  // namespace

// static
KioskAppLauncher* KioskAppLauncher::running_instance_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// KioskAppLauncher::CryptohomedChecker ensures cryptohome daemon is up
// and running by issuing an IsMounted call. If the call does not go through
// and chromeos::DBUS_METHOD_CALL_SUCCESS is not returned, it will retry after
// some time out and at the maximum five times before it gives up. Upon
// success, it resumes the launch by calling KioskAppLauncher::StartMount.

class KioskAppLauncher::CryptohomedChecker
    : public base::SupportsWeakPtr<CryptohomedChecker> {
 public:
  explicit CryptohomedChecker(KioskAppLauncher* launcher)
      : launcher_(launcher),
        retry_count_(0) {
  }
  ~CryptohomedChecker() {}

  void StartCheck() {
    chromeos::DBusThreadManager::Get()->GetCryptohomeClient()->IsMounted(
        base::Bind(&CryptohomedChecker::OnCryptohomeIsMounted,
                   AsWeakPtr()));
  }

 private:
  void OnCryptohomeIsMounted(chromeos::DBusMethodCallStatus call_status,
                             bool is_mounted) {
    if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS) {
      const int kMaxRetryTimes = 5;
      ++retry_count_;
      if (retry_count_ > kMaxRetryTimes) {
        LOG(ERROR) << "Could not talk to cryptohomed for launching kiosk app.";
        ReportCheckResult(KioskAppLaunchError::CRYPTOHOMED_NOT_RUNNING);
        return;
      }

      const int retry_delay_in_milliseconds = 500 * (1 << retry_count_);
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&CryptohomedChecker::StartCheck, AsWeakPtr()),
          base::TimeDelta::FromMilliseconds(retry_delay_in_milliseconds));
      return;
    }

    if (is_mounted)
      LOG(ERROR) << "Cryptohome is mounted before launching kiosk app.";

    // Proceed only when cryptohome is not mounded or running on dev box.
    if (!is_mounted || !base::chromeos::IsRunningOnChromeOS())
      ReportCheckResult(KioskAppLaunchError::NONE);
    else
      ReportCheckResult(KioskAppLaunchError::ALREADY_MOUNTED);
  }

  void ReportCheckResult(KioskAppLaunchError::Error error) {
    if (error == KioskAppLaunchError::NONE)
      launcher_->StartMount();
    else
      launcher_->ReportLaunchResult(error);
  }

  KioskAppLauncher* launcher_;
  int retry_count_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomedChecker);
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppLauncher::ProfileLoader creates or loads the app profile.

class KioskAppLauncher::ProfileLoader : public LoginUtils::Delegate {
 public:
  explicit ProfileLoader(KioskAppLauncher* launcher)
      : launcher_(launcher) {
  }

  virtual ~ProfileLoader() {
    LoginUtils::Get()->DelegateDeleted(this);
  }

  void Start() {
    // TODO(nkostylev): Pass real username_hash here.
    LoginUtils::Get()->PrepareProfile(
        UserContext(GetAppUserNameFromAppId(launcher_->app_id_),
                    std::string(),   // password
                    std::string()),  // auth_code
        std::string(),  // display email
        false,  // using_oauth
        false,  // has_cookies
        this);
  }

 private:
  // LoginUtils::Delegate overrides:
  virtual void OnProfilePrepared(Profile* profile) OVERRIDE {
    launcher_->OnProfilePrepared(profile);
  }

  KioskAppLauncher* launcher_;
  DISALLOW_COPY_AND_ASSIGN(ProfileLoader);
};

////////////////////////////////////////////////////////////////////////////////
// KioskAppLauncher

KioskAppLauncher::KioskAppLauncher(const std::string& app_id)
    : app_id_(app_id),
      remove_attempted_(false) {
}

KioskAppLauncher::~KioskAppLauncher() {}

void KioskAppLauncher::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (running_instance_) {
    LOG(WARNING) << "Unable to launch " << app_id_ << "with a pending launch.";
    ReportLaunchResult(KioskAppLaunchError::HAS_PENDING_LAUNCH);
    return;
  }

  running_instance_ = this;  // Reset in ReportLaunchResult.

  // Check cryptohomed. If all goes good, flow goes to StartMount. Otherwise
  // it goes to ReportLaunchResult with failure.
  crytohomed_checker.reset(new CryptohomedChecker(this));
  crytohomed_checker->StartCheck();
}

void KioskAppLauncher::ReportLaunchResult(KioskAppLaunchError::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  running_instance_ = NULL;

  if (error != KioskAppLaunchError::NONE) {
    // Saves the error and ends the session to go back to login screen.
    KioskAppLaunchError::Save(error);
    chrome::AttemptUserExit();
  }

  delete this;
}

void KioskAppLauncher::StartMount() {
  const std::string token =
      CrosLibrary::Get()->GetCertLibrary()->EncryptWithSystemSalt(app_id_);

  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMount(
      app_id_,
      token,
      cryptohome::CREATE_IF_MISSING,
      base::Bind(&KioskAppLauncher::MountCallback,
                 base::Unretained(this)));
}

void KioskAppLauncher::MountCallback(bool mount_success,
                                     cryptohome::MountError mount_error) {
  if (mount_success) {
    profile_loader_.reset(new ProfileLoader(this));
    profile_loader_->Start();
    return;
  }

  if (!remove_attempted_) {
    LOG(ERROR) << "Attempt to remove app cryptohome because of mount failure"
               << ", mount error=" << mount_error;

    remove_attempted_ = true;
    AttemptRemove();
    return;
  }

  LOG(ERROR) << "Failed to mount app cryptohome, error=" << mount_error;
  ReportLaunchResult(KioskAppLaunchError::UNABLE_TO_MOUNT);
}

void KioskAppLauncher::AttemptRemove() {
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      app_id_,
      base::Bind(&KioskAppLauncher::RemoveCallback,
                 base::Unretained(this)));
}

void KioskAppLauncher::RemoveCallback(bool success,
                                      cryptohome::MountError return_code) {
  if (success) {
    StartMount();
    return;
  }

  LOG(ERROR) << "Failed to remove app cryptohome, erro=" << return_code;
  ReportLaunchResult(KioskAppLaunchError::UNABLE_TO_REMOVE);
}

void KioskAppLauncher::OnProfilePrepared(Profile* profile) {
  // StartupAppLauncher deletes itself when done.
  (new chromeos::StartupAppLauncher(profile, app_id_))->Start(
      chromeos::StartupAppLauncher::LAUNCH_ON_SESSION_START);

  if (BaseLoginDisplayHost::default_host())
    BaseLoginDisplayHost::default_host()->OnSessionStart();
  UserManager::Get()->SessionStarted();

  ReportLaunchResult(KioskAppLaunchError::NONE);
}

}  // namespace chromeos
