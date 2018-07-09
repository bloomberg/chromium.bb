// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UPDATER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UPDATER_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_manager.h"

// The SigninUpdater is responsible for updating renderers about the signin
// change.
class SigninUpdater : public KeyedService, public SigninManagerBase::Observer {
 public:
  explicit SigninUpdater(SigninManagerBase* signin_manager);
  ~SigninUpdater() override;

  // KeyedService:
  void Shutdown() override;

  // SigninManagerBase::Observer:
  void GoogleSigninSucceeded(const AccountInfo& account_info) override;
  void GoogleSignedOut(const AccountInfo& account_info) override;

 private:
  void UpdateRenderer(bool is_signed_in);

  SigninManagerBase* signin_manager_;

  DISALLOW_COPY_AND_ASSIGN(SigninUpdater);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UPDATER_H_
