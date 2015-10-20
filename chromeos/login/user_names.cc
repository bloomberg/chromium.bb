// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/login/user_names.h"

#include "base/memory/singleton.h"
#include "components/signin/core/account_id/account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"

class AccountId;

namespace {

// Username for Demo session user.
constexpr const char kDemoUserName[] = "demouser@demo.app.local";

// This is a singleton object that is used to store several
// global AccountIds for special accounts.
class FixedAccountManager {
 public:
  static FixedAccountManager* GetInstance() {
    return base::Singleton<FixedAccountManager>::get();
  }

  const AccountId& stub_account_id() const { return stub_account_id_; }
  const AccountId& signin_account_id() const { return signin_account_id_; }
  const AccountId& guest_account_id() const { return guest_account_id_; }
  const AccountId& demo_account_id() const { return demo_account_id_; }

 private:
  friend struct base::DefaultSingletonTraits<FixedAccountManager>;

  FixedAccountManager();

  const AccountId stub_account_id_;
  const AccountId signin_account_id_;
  const AccountId guest_account_id_;
  const AccountId demo_account_id_;

  DISALLOW_COPY_AND_ASSIGN(FixedAccountManager);
};

FixedAccountManager::FixedAccountManager()
    : stub_account_id_(AccountId::FromUserEmail(chromeos::login::kStubUser)),
      signin_account_id_(
          AccountId::FromUserEmail(chromeos::login::kSignInUser)),
      guest_account_id_(
          AccountId::FromUserEmail(chromeos::login::kGuestUserName)),
      demo_account_id_(AccountId::FromUserEmail(kDemoUserName)) {}

}  // namespace

namespace chromeos {

namespace login {

constexpr const char kStubUser[] = "stub-user@example.com";

constexpr const char kSignInUser[] = "sign-in-user-id";

// Should match cros constant in platform/libchromeos/chromeos/cryptohome.h
constexpr const char kGuestUserName[] = "$guest";

constexpr const char kSupervisedUserDomain[] = "locally-managed.localhost";

std::string CanonicalizeUserID(const std::string& user_id) {
  if (user_id == chromeos::login::kGuestUserName)
    return user_id;
  return gaia::CanonicalizeEmail(user_id);
}

const AccountId& StubAccountId() {
  return FixedAccountManager::GetInstance()->stub_account_id();
}

const AccountId& SignInAccountId() {
  return FixedAccountManager::GetInstance()->signin_account_id();
}

const AccountId& GuestAccountId() {
  return FixedAccountManager::GetInstance()->guest_account_id();
}

const AccountId& DemoAccountId() {
  return FixedAccountManager::GetInstance()->demo_account_id();
}

}  // namespace login

}  // namespace chromeos
