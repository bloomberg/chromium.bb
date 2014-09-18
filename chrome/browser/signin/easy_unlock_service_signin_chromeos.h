// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_SIGNIN_CHROMEOS_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_SIGNIN_CHROMEOS_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#include "chrome/browser/signin/easy_unlock_service.h"

namespace base {
class DictionaryValue;
class ListValue;
}

// EasyUnlockService instance that should be used for signin profile.
class EasyUnlockServiceSignin : public EasyUnlockService {
 public:
  explicit EasyUnlockServiceSignin(Profile* profile);
  virtual ~EasyUnlockServiceSignin();

  void SetAssociatedUser(const std::string& user_id);

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
  virtual std::string GetChallenge() const OVERRIDE;
  virtual bool IsAllowedInternal() OVERRIDE;
  virtual void InitializeInternal() OVERRIDE;

  // Fetches |remote_devices_| info from cryptohome keys.
  void FetchCryptohomeKeys();

  // Callback invoked when the cryptohome keys are fetched.
  void OnCryptohomeKeysFetched(
      bool success,
      const chromeos::EasyUnlockDeviceKeyDataList& devices);

  // User id of the associated user.
  std::string user_id_;

  // Remote devices of the associated user.
  chromeos::EasyUnlockDeviceKeyDataList remote_devices_;
  scoped_ptr<base::ListValue> remote_devices_value_;

  base::WeakPtrFactory<EasyUnlockServiceSignin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceSignin);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_SIGNIN_CHROMEOS_H_
