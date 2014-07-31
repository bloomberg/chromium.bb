// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_MOCK_AUTHENTICATOR_H_
#define CHROMEOS_LOGIN_AUTH_MOCK_AUTHENTICATOR_H_

#include <string>

#include "base/message_loop/message_loop_proxy.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/login/auth/authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "testing/gtest/include/gtest/gtest.h"

class Profile;

namespace chromeos {

class AuthStatusConsumer;

class CHROMEOS_EXPORT MockAuthenticator : public Authenticator {
 public:
  MockAuthenticator(AuthStatusConsumer* consumer,
                    const UserContext& expected_user_context);

  // Authenticator:
  virtual void CompleteLogin(Profile* profile,
                             const UserContext& user_context) OVERRIDE;
  virtual void AuthenticateToLogin(Profile* profile,
                                   const UserContext& user_context) OVERRIDE;
  virtual void AuthenticateToUnlock(const UserContext& user_context) OVERRIDE;
  virtual void LoginAsSupervisedUser(const UserContext& user_context) OVERRIDE;
  virtual void LoginRetailMode() OVERRIDE;
  virtual void LoginOffTheRecord() OVERRIDE;
  virtual void LoginAsPublicSession(const UserContext& user_context) OVERRIDE;
  virtual void LoginAsKioskAccount(const std::string& app_user_id,
                                   bool use_guest_mount) OVERRIDE;
  virtual void OnRetailModeAuthSuccess() OVERRIDE;
  virtual void OnAuthSuccess() OVERRIDE;
  virtual void OnAuthFailure(const AuthFailure& failure) OVERRIDE;
  virtual void RecoverEncryptedData(const std::string& old_password) OVERRIDE;
  virtual void ResyncEncryptedData() OVERRIDE;

  virtual void SetExpectedCredentials(const UserContext& user_context);

 protected:
  virtual ~MockAuthenticator();

 private:
  UserContext expected_user_context_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MockAuthenticator);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_MOCK_AUTHENTICATOR_H_
