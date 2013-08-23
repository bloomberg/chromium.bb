// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_profile_loader.h"

#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_library.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"

using content::BrowserThread;

namespace chromeos {

namespace {

void IgnoreResult(bool mount_success, cryptohome::MountError mount_error) {}

// Converts a user id to the old non-canonicalized format. We need this
// old user id to cleanup crypthomes from older verisons.
// TODO(tengs): Remove this after all clients migrated to new home.
std::string GetOldUserId(const std::string& user_id) {
  size_t separator_pos = user_id.find('@');
  if (separator_pos != user_id.npos && separator_pos < user_id.length() - 1) {
    std::string username = user_id.substr(0, separator_pos);
    std::string domain = user_id.substr(separator_pos + 1);
    StringToUpperASCII(&username);
    return username + "@" + domain;
  } else {
    LOG(ERROR) << "User id "
        << user_id << " does not seem to be a valid format";
    NOTREACHED();
    return user_id;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// KioskProfileLoader::CryptohomedChecker ensures cryptohome daemon is up
// and running by issuing an IsMounted call. If the call does not go through
// and chromeos::DBUS_METHOD_CALL_SUCCESS is not returned, it will retry after
// some time out and at the maximum five times before it gives up. Upon
// success, it resumes the launch by calling KioskProfileLoader::StartMount.

class KioskProfileLoader::CryptohomedChecker
    : public base::SupportsWeakPtr<CryptohomedChecker> {
 public:
  explicit CryptohomedChecker(KioskProfileLoader* loader)
      : loader_(loader),
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
      base::MessageLoop::current()->PostDelayedTask(
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
      loader_->StartMount();
    else
      loader_->ReportLaunchResult(error);
  }

  KioskProfileLoader* loader_;
  int retry_count_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomedChecker);
};

////////////////////////////////////////////////////////////////////////////////
// KioskProfileLoader::ProfileLoader actually creates or loads the app profile.

class KioskProfileLoader::ProfileLoader : public LoginUtils::Delegate {
 public:
  ProfileLoader(KioskAppManager* kiosk_app_manager,
                KioskProfileLoader* kiosk_profile_loader)
      : kiosk_profile_loader_(kiosk_profile_loader),
        user_id_(kiosk_profile_loader->user_id_) {
    CHECK(!user_id_.empty());
  }

  virtual ~ProfileLoader() {
    LoginUtils::Get()->DelegateDeleted(this);
  }

  void Start() {
    cryptohome::AsyncMethodCaller::GetInstance()->AsyncGetSanitizedUsername(
        user_id_,
        base::Bind(&ProfileLoader::OnUsernameHashRetrieved,
                   base::Unretained(this)));
  }

 private:
  void OnUsernameHashRetrieved(bool success,
                               const std::string& username_hash) {
    if (!success) {
      LOG(ERROR) << "Unable to retrieve username hash for user '" << user_id_
                 << "'.";
      kiosk_profile_loader_->ReportLaunchResult(
          KioskAppLaunchError::UNABLE_TO_RETRIEVE_HASH);
      return;
    }
    LoginUtils::Get()->PrepareProfile(
        UserContext(user_id_,
                    std::string(),   // password
                    std::string(),   // auth_code
                    username_hash),
        std::string(),  // display email
        false,  // using_oauth
        false,  // has_cookies
        false,  // has_active_session
        this);
  }

  // LoginUtils::Delegate overrides:
  virtual void OnProfilePrepared(Profile* profile) OVERRIDE {
    kiosk_profile_loader_->OnProfilePrepared(profile);
  }

  KioskProfileLoader* kiosk_profile_loader_;
  std::string user_id_;

  DISALLOW_COPY_AND_ASSIGN(ProfileLoader);
};

////////////////////////////////////////////////////////////////////////////////
// KioskProfileLoader

KioskProfileLoader::KioskProfileLoader(KioskAppManager* kiosk_app_manager,
                                       const std::string& app_id,
                                       Delegate* delegate)
    : kiosk_app_manager_(kiosk_app_manager),
      app_id_(app_id),
      delegate_(delegate),
      remove_attempted_(false) {
  KioskAppManager::App app;
  CHECK(kiosk_app_manager_->GetApp(app_id_, &app));
  user_id_ = app.user_id;
}

KioskProfileLoader::~KioskProfileLoader() {}

void KioskProfileLoader::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check cryptohomed. If all goes good, flow goes to StartMount. Otherwise
  // it goes to ReportLaunchResult with failure.
  crytohomed_checker.reset(new CryptohomedChecker(this));
  crytohomed_checker->StartCheck();
}

void KioskProfileLoader::ReportLaunchResult(KioskAppLaunchError::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != KioskAppLaunchError::NONE) {
    delegate_->OnProfileLoadFailed(error);
  }
}

void KioskProfileLoader::StartMount() {
  // Nuke old home that uses |app_id_| as cryptohome user id.
  // TODO(xiyuan): Remove this after all clients migrated to new home.
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      app_id_,
      base::Bind(&IgnoreResult));

  // Nuke old home that uses non-canonicalized user id.
  // TODO(tengs): Remove this after all clients migrated to new home.
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      GetOldUserId(user_id_),
      base::Bind(&IgnoreResult));

  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMountPublic(
      user_id_,
      cryptohome::CREATE_IF_MISSING,
      base::Bind(&KioskProfileLoader::MountCallback,
                 base::Unretained(this)));
}

void KioskProfileLoader::MountCallback(bool mount_success,
                                       cryptohome::MountError mount_error) {
  if (mount_success) {
    profile_loader_.reset(new ProfileLoader(kiosk_app_manager_, this));
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

void KioskProfileLoader::AttemptRemove() {
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      user_id_,
      base::Bind(&KioskProfileLoader::RemoveCallback,
                 base::Unretained(this)));
}

void KioskProfileLoader::RemoveCallback(bool success,
                                      cryptohome::MountError return_code) {
  if (success) {
    StartMount();
    return;
  }

  LOG(ERROR) << "Failed to remove app cryptohome, error=" << return_code;
  ReportLaunchResult(KioskAppLaunchError::UNABLE_TO_REMOVE);
}

void KioskProfileLoader::OnProfilePrepared(Profile* profile) {
  UserManager::Get()->SessionStarted();
  delegate_->OnProfileLoaded(profile);
  ReportLaunchResult(KioskAppLaunchError::NONE);
}

}  // namespace chromeos
