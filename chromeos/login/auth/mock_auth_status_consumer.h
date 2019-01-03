// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_MOCK_AUTH_STATUS_CONSUMER_H_
#define CHROMEOS_LOGIN_AUTH_MOCK_AUTH_STATUS_CONSUMER_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/user_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) MockAuthStatusConsumer
    : public AuthStatusConsumer {
 public:
  explicit MockAuthStatusConsumer(base::OnceClosure quit_closure);
  virtual ~MockAuthStatusConsumer();

  MOCK_METHOD1(OnAuthFailure, void(const AuthFailure& error));
  MOCK_METHOD1(OnRetailModeAuthSuccess, void(const UserContext& user_context));
  MOCK_METHOD1(OnAuthSuccess, void(const UserContext& user_context));
  MOCK_METHOD0(OnOffTheRecordAuthSuccess, void(void));
  MOCK_METHOD0(OnPasswordChangeDetected, void(void));

  // The following functions can be used in gmock Invoke() clauses.

  // Compatible with AuthStatusConsumer::OnRetailModeAuthSuccess()
  void OnRetailModeSuccessQuit(const UserContext& user_context);
  void OnRetailModeSuccessQuitAndFail(const UserContext& user_context);

  // Compatible with AuthStatusConsumer::OnOffTheRecordAuthSuccess()
  void OnGuestSuccessQuit();
  void OnGuestSuccessQuitAndFail();

  // Compatible with AuthStatusConsumer::OnAuthSuccess()
  void OnSuccessQuit(const UserContext& user_context);
  void OnSuccessQuitAndFail(const UserContext& user_context);

  // Compatible with AuthStatusConsumer::OnAuthFailure()
  void OnFailQuit(const AuthFailure& error);
  void OnFailQuitAndFail(const AuthFailure& error);

  // Compatible with AuthStatusConsumer::OnPasswordChangeDetected()
  void OnMigrateQuit();
  void OnMigrateQuitAndFail();

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_MOCK_AUTH_STATUS_CONSUMER_H_
