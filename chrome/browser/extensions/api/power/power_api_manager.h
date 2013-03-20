// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_POWER_POWER_API_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_POWER_POWER_API_MANAGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/common/extensions/api/power.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/power_save_blocker.h"

namespace extensions {

class PowerApiManager : public content::NotificationObserver {
 public:
  typedef base::Callback<scoped_ptr<content::PowerSaveBlocker>(
      content::PowerSaveBlocker::PowerSaveBlockerType,
      const std::string&)> CreateBlockerFunction;

  static PowerApiManager* GetInstance();

  // Adds an extension lock at |level| for |extension_id|, replacing the
  // extension's existing lock, if any.
  void AddRequest(const std::string& extension_id, api::power::Level level);

  // Removes an extension lock for an extension. Calling this for an
  // extension id without a lock will do nothing.
  void RemoveRequest(const std::string& extension_id);

  // Replaces the function that will be called to create PowerSaveBlocker
  // objects.  Passing an empty callback will revert to the default.
  void SetCreateBlockerFunctionForTesting(CreateBlockerFunction function);

  // Overridden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<PowerApiManager>;

  PowerApiManager();
  virtual ~PowerApiManager();

  // Updates |power_save_blocker_| and |current_level_| after iterating
  // over |extension_levels_|.
  void UpdatePowerSaveBlocker();

  content::NotificationRegistrar registrar_;

  // Function that should be called to create PowerSaveBlocker objects.
  // Tests can change this to record what would've been done instead of
  // actually changing the system power-saving settings.
  CreateBlockerFunction create_blocker_function_;

  scoped_ptr<content::PowerSaveBlocker> power_save_blocker_;

  // Current level used by |power_save_blocker_|.  Meaningless if
  // |power_save_blocker_| is NULL.
  api::power::Level current_level_;

  // Map from extension ID to the corresponding level for each extension
  // that has an outstanding request.
  typedef std::map<std::string, api::power::Level> ExtensionLevelMap;
  ExtensionLevelMap extension_levels_;

  DISALLOW_COPY_AND_ASSIGN(PowerApiManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_POWER_POWER_API_MANAGER_H_
