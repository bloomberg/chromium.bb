// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_UPDATE_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_UPDATE_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/timer.h"
#include "chrome/browser/extensions/update_observer.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace chromeos {

// This class enforces automatic restart on app and Chrome updates in app mode.
class KioskAppUpdateService : public ProfileKeyedService,
                              public extensions::UpdateObserver {
 public:
  explicit KioskAppUpdateService(Profile* profile);
  virtual ~KioskAppUpdateService();

  void set_app_id(const std::string& app_id) { app_id_ = app_id; }
  std::string get_app_id() const { return app_id_; }

 private:
  void StartRestartTimer();
  void ForceRestart();

  // extensions::UpdateObserver overrides:
  virtual void OnAppUpdateAvailable(const std::string& app_id) OVERRIDE;
  virtual void OnChromeUpdateAvailable() OVERRIDE {}

  // ProfileKeyedService overrides:
  virtual void Shutdown() OVERRIDE;

 private:
  Profile* profile_;
  std::string app_id_;

  // After we detect an upgrade we start a one-short timer to force restart.
  base::OneShotTimer<KioskAppUpdateService> restart_timer_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppUpdateService);
};

// Singleton that owns all KioskAppUpdateServices and associates them with
// profiles.
class KioskAppUpdateServiceFactory: public ProfileKeyedServiceFactory {
 public:
  // Returns the KioskAppUpdateService for |profile|, creating it if it is not
  // yet created.
  static KioskAppUpdateService* GetForProfile(Profile* profile);

  // Returns the KioskAppUpdateServiceFactory instance.
  static KioskAppUpdateServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<KioskAppUpdateServiceFactory>;

  KioskAppUpdateServiceFactory();
  virtual ~KioskAppUpdateServiceFactory();

  // ProfileKeyedServiceFactory overrides:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_UPDATE_SERVICE_H_
