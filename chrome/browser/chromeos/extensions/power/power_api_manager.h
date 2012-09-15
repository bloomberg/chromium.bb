// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_POWER_POWER_API_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_POWER_POWER_API_MANAGER_H_

#include <set>
#include <string>

#include "base/memory/singleton.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {
class PowerStateOverride;
}

namespace extensions {
namespace power {

class PowerApiManager : public content::NotificationObserver {
 public:
  static PowerApiManager* GetInstance();

  // Add an extension lock for an extension. Calling this for an
  // extension id already with a lock will do nothing.
  void AddExtensionLock(const std::string& extension_id);

  // Remove an extension lock for an extension. Calling this for an
  // extension id without a lock will do nothing.
  void RemoveExtensionLock(const std::string& extension_id);

  // Overridden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<PowerApiManager>;

  // Singleton class - use GetInstance instead.
  PowerApiManager();
  virtual ~PowerApiManager();

  // Check the state of the set of extension IDs with a lock.
  // If we have any extensions with a lock, make sure that power management
  // is overriden. If no extensions have a lock, ensure that our power
  // management override is released.
  void UpdatePowerSettings();

  content::NotificationRegistrar registrar_;

  scoped_ptr<chromeos::PowerStateOverride> power_state_override_;

  // Set of extension IDs that have a keep awake lock.
  std::set<std::string> extension_ids_set_;

  DISALLOW_COPY_AND_ASSIGN(PowerApiManager);
};

}  // namespace power
}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_POWER_POWER_API_MANAGER_H_
