// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_POWER_POWER_API_MANAGER_H_
#define EXTENSIONS_BROWSER_API_POWER_POWER_API_MANAGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/power_save_blocker.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/api/power.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Handles calls made via the chrome.power API. There is a separate instance of
// this class for each profile, as requests are tracked by extension ID, but a
// regular and incognito profile will share the same instance.
// TODO(derat): Move this to power_api.h and rename it to PowerApi.
class PowerApiManager : public BrowserContextKeyedAPI,
                        public extensions::ExtensionRegistryObserver {
 public:
  typedef base::Callback<scoped_ptr<content::PowerSaveBlocker>(
      content::PowerSaveBlocker::PowerSaveBlockerType,
      const std::string&)> CreateBlockerFunction;

  static PowerApiManager* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PowerApiManager>* GetFactoryInstance();

  // Adds an extension lock at |level| for |extension_id|, replacing the
  // extension's existing lock, if any.
  void AddRequest(const std::string& extension_id,
                  core_api::power::Level level);

  // Removes an extension lock for an extension. Calling this for an
  // extension id without a lock will do nothing.
  void RemoveRequest(const std::string& extension_id);

  // Replaces the function that will be called to create PowerSaveBlocker
  // objects.  Passing an empty callback will revert to the default.
  void SetCreateBlockerFunctionForTesting(CreateBlockerFunction function);

  // Overridden from extensions::ExtensionRegistryObserver.
  virtual void OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   UnloadedExtensionInfo::Reason reason)
      OVERRIDE;

 private:
  friend class BrowserContextKeyedAPIFactory<PowerApiManager>;

  explicit PowerApiManager(content::BrowserContext* context);
  virtual ~PowerApiManager();

  // Updates |power_save_blocker_| and |current_level_| after iterating
  // over |extension_levels_|.
  void UpdatePowerSaveBlocker();

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "PowerApiManager"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsCreatedWithBrowserContext = false;
  virtual void Shutdown() OVERRIDE;

  content::BrowserContext* browser_context_;

  // Function that should be called to create PowerSaveBlocker objects.
  // Tests can change this to record what would've been done instead of
  // actually changing the system power-saving settings.
  CreateBlockerFunction create_blocker_function_;

  scoped_ptr<content::PowerSaveBlocker> power_save_blocker_;

  // Current level used by |power_save_blocker_|.  Meaningless if
  // |power_save_blocker_| is NULL.
  core_api::power::Level current_level_;

  // Map from extension ID to the corresponding level for each extension
  // that has an outstanding request.
  typedef std::map<std::string, core_api::power::Level> ExtensionLevelMap;
  ExtensionLevelMap extension_levels_;

  DISALLOW_COPY_AND_ASSIGN(PowerApiManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_POWER_POWER_API_MANAGER_H_
