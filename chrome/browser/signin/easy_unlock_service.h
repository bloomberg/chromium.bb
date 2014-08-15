// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class EasyUnlockScreenlockStateHandler;
class EasyUnlockServiceObserver;
class EasyUnlockToggleFlow;
class Profile;

class EasyUnlockService : public KeyedService {
 public:
  enum TurnOffFlowStatus {
    IDLE,
    PENDING,
    FAIL,
  };

  explicit EasyUnlockService(Profile* profile);
  virtual ~EasyUnlockService();

  // Gets EasyUnlockService instance.
  static EasyUnlockService* Get(Profile* profile);

  // Registers Easy Unlock profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Launches Easy Unlock Setup app.
  void LaunchSetup();

  // Whether easy unlock is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted either the flag is enabled or its field trial is enabled.
  bool IsAllowed();

  // Gets |screenlock_state_handler_|. Returns NULL if Easy Unlock is not
  // allowed. Otherwise, if |screenlock_state_handler_| is not set, an instance
  // is created. Do not cache the returned value, as it may go away if Easy
  // Unlock gets disabled.
  EasyUnlockScreenlockStateHandler* GetScreenlockStateHandler();

  // Gets/Sets/Clears the permit access for the local device.
  const base::DictionaryValue* GetPermitAccess() const;
  void SetPermitAccess(const base::DictionaryValue& permit);
  void ClearPermitAccess();

  // Gets/Sets/Clears the remote devices list.
  const base::ListValue* GetRemoteDevices() const;
  void SetRemoteDevices(const base::ListValue& devices);
  void ClearRemoteDevices();

  void RunTurnOffFlow();
  void ResetTurnOffFlow();

  void AddObserver(EasyUnlockServiceObserver* observer);
  void RemoveObserver(EasyUnlockServiceObserver* observer);

  TurnOffFlowStatus turn_off_flow_status() const {
    return turn_off_flow_status_;
  }

 private:
  void Initialize();
  void LoadApp();
  void UnloadApp();
  void OnPrefsChanged();

  void SetTurnOffFlowStatus(TurnOffFlowStatus status);
  void OnTurnOffFlowFinished(bool success);

  Profile* profile_;
  PrefChangeRegistrar registrar_;
  // Created lazily in |GetScreenlockStateHandler|.
  scoped_ptr<EasyUnlockScreenlockStateHandler> screenlock_state_handler_;

  TurnOffFlowStatus turn_off_flow_status_;
  scoped_ptr<EasyUnlockToggleFlow> turn_off_flow_;
  ObserverList<EasyUnlockServiceObserver> observers_;

  base::WeakPtrFactory<EasyUnlockService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockService);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_
