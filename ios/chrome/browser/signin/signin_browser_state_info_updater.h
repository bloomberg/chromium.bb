// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_STATE_INFO_UPDATER_H_
#define IOS_CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_STATE_INFO_UPDATER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager_base.h"

// This class listens to various signin events and updates the signin-related
// fields of BrowserStateInfoCache.
class SigninBrowserStateInfoUpdater : public KeyedService,
                                      public SigninErrorController::Observer,
                                      public SigninManagerBase::Observer {
 public:
  SigninBrowserStateInfoUpdater(SigninManagerBase* signin_manager,
                                SigninErrorController* signin_error_controller,
                                const base::FilePath& browser_state_path);

  ~SigninBrowserStateInfoUpdater() override;

 private:
  // KeyedService:
  void Shutdown() override;

  // SigninErrorController::Observer:
  void OnErrorChanged() override;

  // SigninManagerBase::Observer:
  void GoogleSigninSucceeded(const AccountInfo& account_info) override;
  void GoogleSignedOut(const AccountInfo& account_info) override;

  SigninErrorController* signin_error_controller_ = nullptr;
  const base::FilePath browser_state_path_;
  ScopedObserver<SigninErrorController, SigninBrowserStateInfoUpdater>
      signin_error_controller_observer_;
  ScopedObserver<SigninManagerBase, SigninBrowserStateInfoUpdater>
      signin_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(SigninBrowserStateInfoUpdater);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_STATE_INFO_UPDATER_H_
