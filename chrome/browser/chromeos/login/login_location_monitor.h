// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_LOCATION_MONITOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_LOCATION_MONITOR_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/geolocation_provider.h"

namespace chromeos {

// This class does one-time Geoposition resolve for OOBE.
// It stops after first valid position received or when timeout occurs.
// Valid position is sent to WizardController to resolve timezone.
class LoginLocationMonitor {
 public:
  ~LoginLocationMonitor();

  // Called on UI thread.
  static void InstallLocationCallback(const base::TimeDelta timeout);
  static void RemoveLocationCallback();

 private:
  friend struct DefaultSingletonTraits<LoginLocationMonitor>;
  friend class Singleton<chromeos::LoginLocationMonitor>;

  LoginLocationMonitor();

  // Called on UI thread.
  static LoginLocationMonitor* GetInstance();

  // Called on IO thread.
  static void DoInstallLocationCallback(
      content::GeolocationProvider::LocationUpdateCallback callback);
  static void DoRemoveLocationCallback(
      content::GeolocationProvider::LocationUpdateCallback callback);

  // OnLocationUpdated comes on IO thread and should be passed to UI thread.
  static void OnLocationUpdatedIO(const content::Geoposition& position);
  static void OnLocationUpdatedUI(const content::Geoposition& position);

  base::WeakPtrFactory<LoginLocationMonitor> weak_factory_;
  content::GeolocationProvider::LocationUpdateCallback on_location_update_;
  base::OneShotTimer<LoginLocationMonitor> request_timeout_;

  base::Time started_;

  DISALLOW_COPY_AND_ASSIGN(LoginLocationMonitor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_LOCATION_MONITOR_H_
