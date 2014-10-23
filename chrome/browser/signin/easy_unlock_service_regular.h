// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/signin/easy_unlock_service.h"

namespace base {
class DictionaryValue;
class ListValue;
}

class EasyUnlockToggleFlow;
class Profile;

// EasyUnlockService instance that should be used for regular, non-signin
// profiles.
class EasyUnlockServiceRegular : public EasyUnlockService {
 public:
  explicit EasyUnlockServiceRegular(Profile* profile);
  ~EasyUnlockServiceRegular() override;

 private:
  // EasyUnlockService implementation.
  EasyUnlockService::Type GetType() const override;
  std::string GetUserEmail() const override;
  void LaunchSetup() override;
  const base::DictionaryValue* GetPermitAccess() const override;
  void SetPermitAccess(const base::DictionaryValue& permit) override;
  void ClearPermitAccess() override;
  const base::ListValue* GetRemoteDevices() const override;
  void SetRemoteDevices(const base::ListValue& devices) override;
  void ClearRemoteDevices() override;
  void RunTurnOffFlow() override;
  void ResetTurnOffFlow() override;
  TurnOffFlowStatus GetTurnOffFlowStatus() const override;
  std::string GetChallenge() const override;
  std::string GetWrappedSecret() const override;
  void RecordEasySignInOutcome(const std::string& user_id,
                               bool success) const override;
  void RecordPasswordLoginEvent(const std::string& user_id) const override;
  void InitializeInternal() override;
  void ShutdownInternal() override;
  bool IsAllowedInternal() override;

  // Callback when the controlling pref changes.
  void OnPrefsChanged();

  // Sets the new turn-off flow status.
  void SetTurnOffFlowStatus(TurnOffFlowStatus status);

  // Callback invoked when turn off flow has finished.
  void OnTurnOffFlowFinished(bool success);

  PrefChangeRegistrar registrar_;

  TurnOffFlowStatus turn_off_flow_status_;
  scoped_ptr<EasyUnlockToggleFlow> turn_off_flow_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceRegular);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_REGULAR_H_
