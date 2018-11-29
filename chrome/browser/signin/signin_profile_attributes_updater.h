// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager_base.h"

// This class listens to various signin events and updates the signin-related
// fields of ProfileAttributes.
class SigninProfileAttributesUpdater : public KeyedService,
                                       public SigninErrorController::Observer,
                                       public SigninManagerBase::Observer {
 public:
  SigninProfileAttributesUpdater(SigninManagerBase* signin_manager,
                                 SigninErrorController* signin_error_controller,
                                 const base::FilePath& profile_path);

  ~SigninProfileAttributesUpdater() override;

 private:
  // KeyedService:
  void Shutdown() override;

  // SigninErrorController::Observer:
  void OnErrorChanged() override;

// These observer methods are never called on ChromeOS.
#if !defined(OS_CHROMEOS)
  // SigninManagerBase::Observer:
  void GoogleSigninSucceeded(const AccountInfo& account_info) override;
  void GoogleSignedOut(const AccountInfo& account_info) override;
#endif

  SigninErrorController* signin_error_controller_;
  const base::FilePath profile_path_;
  ScopedObserver<SigninErrorController, SigninProfileAttributesUpdater>
      signin_error_controller_observer_;
  ScopedObserver<SigninManagerBase, SigninProfileAttributesUpdater>
      signin_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(SigninProfileAttributesUpdater);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROFILE_ATTRIBUTES_UPDATER_H_
