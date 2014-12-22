// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"

namespace chromeos {

class LoginUtilsImpl : public LoginUtils,
                       public UserSessionManagerDelegate {
 public:
  LoginUtilsImpl()
      : delegate_(NULL) {
  }

  virtual ~LoginUtilsImpl() {
  }

  // LoginUtils implementation:
  virtual void DoBrowserLaunch(Profile* profile,
                               LoginDisplayHost* login_host) override;
  virtual void PrepareProfile(
      const UserContext& user_context,
      bool has_auth_cookies,
      bool has_active_session,
      LoginUtils::Delegate* delegate) override;
  virtual void DelegateDeleted(LoginUtils::Delegate* delegate) override;
  virtual scoped_refptr<Authenticator> CreateAuthenticator(
      AuthStatusConsumer* consumer) override;

  // UserSessionManager::Delegate implementation:
  virtual void OnProfilePrepared(Profile* profile,
                                 bool browser_launched) override;

 private:
  // Has to be scoped_refptr, see comment for CreateAuthenticator(...).
  scoped_refptr<Authenticator> authenticator_;

  // Delegate to be fired when the profile will be prepared.
  LoginUtils::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsImpl);
};

class LoginUtilsWrapper {
 public:
  static LoginUtilsWrapper* GetInstance() {
    return Singleton<LoginUtilsWrapper>::get();
  }

  LoginUtils* get() {
    base::AutoLock create(create_lock_);
    if (!ptr_.get())
      reset(new LoginUtilsImpl);
    return ptr_.get();
  }

  void reset(LoginUtils* ptr) {
    ptr_.reset(ptr);
  }

 private:
  friend struct DefaultSingletonTraits<LoginUtilsWrapper>;

  LoginUtilsWrapper() {}

  base::Lock create_lock_;
  scoped_ptr<LoginUtils> ptr_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsWrapper);
};

void LoginUtilsImpl::DoBrowserLaunch(Profile* profile,
                                     LoginDisplayHost* login_host) {
  UserSessionManager::GetInstance()->DoBrowserLaunch(profile, login_host);
}

void LoginUtilsImpl::PrepareProfile(
    const UserContext& user_context,
    bool has_auth_cookies,
    bool has_active_session,
    LoginUtils::Delegate* delegate) {
  // TODO(nkostylev): We have to initialize LoginUtils delegate as long
  // as it coexist with SessionManager.
  delegate_ = delegate;

  UserSessionManager::StartSessionType start_session_type =
      UserAddingScreen::Get()->IsRunning() ?
          UserSessionManager::SECONDARY_USER_SESSION :
          UserSessionManager::PRIMARY_USER_SESSION;

  // For the transition part LoginUtils will just delegate profile
  // creation and initialization to SessionManager. Later LoginUtils will be
  // removed and all LoginUtils clients will just work with SessionManager
  // directly.
  UserSessionManager::GetInstance()->StartSession(user_context,
                                                  start_session_type,
                                                  authenticator_,
                                                  has_auth_cookies,
                                                  has_active_session,
                                                  this);
}

void LoginUtilsImpl::DelegateDeleted(LoginUtils::Delegate* delegate) {
  if (delegate_ == delegate)
    delegate_ = NULL;
}

scoped_refptr<Authenticator> LoginUtilsImpl::CreateAuthenticator(
    AuthStatusConsumer* consumer) {
  // Screen locker needs new Authenticator instance each time.
  if (ScreenLocker::default_screen_locker()) {
    if (authenticator_.get())
      authenticator_->SetConsumer(NULL);
    authenticator_ = NULL;
  }

  if (authenticator_.get() == NULL) {
    authenticator_ = new ChromeCryptohomeAuthenticator(consumer);
  } else {
    // TODO(nkostylev): Fix this hack by improving Authenticator dependencies.
    authenticator_->SetConsumer(consumer);
  }
  return authenticator_;
}

void LoginUtilsImpl::OnProfilePrepared(Profile* profile,
                                       bool browser_launched) {
  if (delegate_)
    delegate_->OnProfilePrepared(profile, browser_launched);
}

// static
LoginUtils* LoginUtils::Get() {
  return LoginUtilsWrapper::GetInstance()->get();
}

// static
void LoginUtils::Set(LoginUtils* mock) {
  LoginUtilsWrapper::GetInstance()->reset(mock);
}

// static
bool LoginUtils::IsWhitelisted(const std::string& username,
                               bool* wildcard_match) {
  // Skip whitelist check for tests.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kOobeSkipPostLogin)) {
    return true;
  }

  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return true;
  return cros_settings->FindEmailInList(
      kAccountsPrefUsers, username, wildcard_match);
}

}  // namespace chromeos
