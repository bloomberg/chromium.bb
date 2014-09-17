// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_SIGNIN_CHROMEOS_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_SIGNIN_CHROMEOS_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/signin/easy_unlock_service.h"

// EasyUnlockService instance that should be used for signin profile.
class EasyUnlockServiceSignin : public EasyUnlockService {
 public:
  explicit EasyUnlockServiceSignin(Profile* profile);
  virtual ~EasyUnlockServiceSignin();

 private:
  // EasyUnlockService implementation:
  virtual EasyUnlockService::Type GetType() const OVERRIDE;
  virtual std::string GetUserEmail() const OVERRIDE;
  virtual void LaunchSetup() OVERRIDE;
  virtual const base::DictionaryValue* GetPermitAccess() const OVERRIDE;
  virtual void SetPermitAccess(const base::DictionaryValue& permit) OVERRIDE;
  virtual void ClearPermitAccess() OVERRIDE;
  virtual const base::ListValue* GetRemoteDevices() const OVERRIDE;
  virtual void SetRemoteDevices(const base::ListValue& devices) OVERRIDE;
  virtual void ClearRemoteDevices() OVERRIDE;
  virtual void RunTurnOffFlow() OVERRIDE;
  virtual void ResetTurnOffFlow() OVERRIDE;
  virtual TurnOffFlowStatus GetTurnOffFlowStatus() const OVERRIDE;
  virtual bool IsAllowedInternal() OVERRIDE;
  virtual void InitializeInternal() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceSignin);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_SIGNIN_CHROMEOS_H_
