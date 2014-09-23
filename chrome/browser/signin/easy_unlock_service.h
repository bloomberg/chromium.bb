// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
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
class Profile;

class EasyUnlockService : public KeyedService {
 public:
  enum TurnOffFlowStatus {
    IDLE,
    PENDING,
    FAIL,
  };

  enum Type {
    TYPE_REGULAR,
    TYPE_SIGNIN
  };

  // Gets EasyUnlockService instance.
  static EasyUnlockService* Get(Profile* profile);

  // Registers Easy Unlock profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the EasyUnlockService type.
  virtual Type GetType() const = 0;

  // Returns the user currently associated with the service.
  virtual std::string GetUserEmail() const = 0;

  // Launches Easy Unlock Setup app.
  virtual void LaunchSetup() = 0;

  // Gets/Sets/Clears the permit access for the local device.
  virtual const base::DictionaryValue* GetPermitAccess() const = 0;
  virtual void SetPermitAccess(const base::DictionaryValue& permit) = 0;
  virtual void ClearPermitAccess() = 0;

  // Gets/Sets/Clears the remote devices list.
  virtual const base::ListValue* GetRemoteDevices() const = 0;
  virtual void SetRemoteDevices(const base::ListValue& devices) = 0;
  virtual void ClearRemoteDevices() = 0;

  // Runs the flow for turning Easy unlock off.
  virtual void RunTurnOffFlow() = 0;

  // Resets the turn off flow if one is in progress.
  virtual void ResetTurnOffFlow() = 0;

  // Returns the current turn off flow status.
  virtual TurnOffFlowStatus GetTurnOffFlowStatus() const = 0;

  // Gets the challenge bytes for the currently associated user.
  virtual std::string GetChallenge() const = 0;

  // Gets |screenlock_state_handler_|. Returns NULL if Easy Unlock is not
  // allowed. Otherwise, if |screenlock_state_handler_| is not set, an instance
  // is created. Do not cache the returned value, as it may go away if Easy
  // Unlock gets disabled.
  EasyUnlockScreenlockStateHandler* GetScreenlockStateHandler();

  // Whether easy unlock is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted either the flag is enabled or its field trial is enabled.
  bool IsAllowed();

  void AddObserver(EasyUnlockServiceObserver* observer);
  void RemoveObserver(EasyUnlockServiceObserver* observer);

 protected:
  explicit EasyUnlockService(Profile* profile);
  virtual ~EasyUnlockService();

  // Does a service type specific initialization.
  virtual void InitializeInternal() = 0;

  // Does a service type specific shutdown. Called from |Shutdown|.
  virtual void ShutdownInternal() = 0;

  // Service type specific tests for whether the service is allowed. Returns
  // false if service is not allowed. If true is returned, the service may still
  // not be allowed if common tests fail (e.g. if Bluetooth is not available).
  virtual bool IsAllowedInternal() = 0;

  // KeyedService override:
  virtual void Shutdown() OVERRIDE;

  // Exposes the profile to which the service is attached to subclasses.
  Profile* profile() const { return profile_; }

  // Installs the Easy unlock component app if it isn't installed and enables
  // the app if it is disabled.
  void LoadApp();

  // Disables the Easy unlock component app if it's loaded.
  void DisableAppIfLoaded();

  // Unloads the Easy unlock component app if it's loaded.
  void UnloadApp();

  // Reloads the Easy unlock component app if it's loaded.
  void ReloadApp();

  // Checks whether Easy unlock should be running and updates app state.
  void UpdateAppState();

  // Notifies the easy unlock app that the user state has been updated.
  void NotifyUserUpdated();

  // Notifies observers that the turn off flow status changed.
  void NotifyTurnOffOperationStatusChanged();

  // Resets |screenlock_state_handler_|.
  void ResetScreenlockStateHandler();

 private:
  // A class to detect whether a bluetooth adapter is present.
  class BluetoothDetector;

  // Initializes the service after ExtensionService is ready.
  void Initialize();

  // Callback when Bluetooth adapter present state changes.
  void OnBluetoothAdapterPresentChanged();

  Profile* profile_;

  // Created lazily in |GetScreenlockStateHandler|.
  scoped_ptr<EasyUnlockScreenlockStateHandler> screenlock_state_handler_;

  scoped_ptr<BluetoothDetector> bluetooth_detector_;

#if defined(OS_CHROMEOS)
  // Monitors suspend and wake state of ChromeOS.
  class PowerMonitor;
  scoped_ptr<PowerMonitor> power_monitor_;
#endif

  // Whether the service has been shut down.
  bool shut_down_;

  ObserverList<EasyUnlockServiceObserver> observers_;

  base::WeakPtrFactory<EasyUnlockService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockService);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_
