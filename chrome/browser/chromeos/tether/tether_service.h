// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

class TetherService : public KeyedService {
 public:
  explicit TetherService(Profile* profile);
  ~TetherService() override;

  // Gets TetherService instance.
  static TetherService* Get(Profile* profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Start the Tether module. Should only be called once a user is logged in.
  void StartTether();

  // Whether Tether is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted if the flag is enabled. Virtual to allow override for testing.
  virtual bool IsAllowed() const;

  // Whether Tether is enabled. Virtual to allow override for testing.
  virtual bool IsEnabled() const;

 protected:
  // KeyedService
  void Shutdown() override;

 private:
  // Callback when the controlling pref changes.
  void OnPrefsChanged();

  Profile* profile_;
  PrefChangeRegistrar registrar_;

  // Whether the service has been shut down.
  bool shut_down_;

  DISALLOW_COPY_AND_ASSIGN(TetherService);
};

#endif  // CHROME_BROWSER_CHROMEOS_TETHER_TETHER_SERVICE_H_
