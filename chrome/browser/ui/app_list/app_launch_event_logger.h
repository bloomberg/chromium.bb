// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LAUNCH_EVENT_LOGGER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LAUNCH_EVENT_LOGGER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ui/app_list/app_launch_event_logger.pb.h"
#include "extensions/browser/extension_registry.h"

namespace app_list {

// This class logs metrics associated with clicking on apps in ChromeOS.
// Logging is restricted to Arc apps with sync enabled, Chrome apps from the
// app store and PWAs from a whitelist. This class uses UKM for logging,
// however, the metrics are not keyed by navigational urls. Instead, for Chrome
// apps the keys are based upon the app id, for Arc apps the keys are based upon
// a hash of the package name and for PWAs the keys are the urls associated with
// the PWA.
class AppLaunchEventLogger {
 public:
  AppLaunchEventLogger();
  ~AppLaunchEventLogger();
  // Processes a click on an app in the suggestion chip and logs the resulting
  // metrics in UKM.
  void OnSuggestionChipClicked(const std::string& id, int suggestion_index);
  // Processes a click on an app located in the grid of apps in the launcher and
  // logs the resulting metrics in UKM.
  void OnGridClicked(const std::string& id);
  // Provides values to be used when testing.
  void SetAppDataForTesting(extensions::ExtensionRegistry* registry,
                            base::DictionaryValue* arc_apps,
                            base::DictionaryValue* arc_packages);

  static const char kPackageName[];
  static const char kShouldSync[];

 private:
  // Remove any leading "chrome-extension://" or "arc://". Also remove any
  // trailing "/".
  std::string RemoveScheme(const std::string& id);
  // Creates the mapping from PWA app id to PWA url. This mapping also acts as a
  // whitelist for which PWA apps can be logged here.
  void PopulatePwaIdUrlMap();
  // Gets the PWA url from its app id. Returns base::EmptyString() if no match
  // found.
  const std::string& GetPwaUrl(const std::string& id);
  // Sets the |event|'s app type based on the id. Also sets the url for PWA apps
  // and package name for Arc apps.
  // Logging policy is enforced here by only assigning a meaningful app type to
  // apps that are allowed to be logged. An app type of OTHER is assigned to
  // apps that cannot be logged by policy.
  void SetAppInfo(AppLaunchEvent* event);
  // Loads the information used to determine which apps can be logged.
  void LoadInstalledAppInformation();
  // Gets the Arc package name for synced apps. Returns base::EmptyString() if
  // app not being synced or app id not found.
  std::string GetSyncedArcPackage(const std::string& id);
  bool IsChromeAppFromWebstore(const std::string&);
  // Logs the app click using UKM.
  void Log(AppLaunchEvent app_launch_event);

  // Map from PWA app id to PWA url. This also acts as a whitelist of PWA apps
  // the can be logged.
  base::flat_map<std::string, std::string> pwa_id_url_map_;

  // The arc apps installed on the device.
  const base::DictionaryValue* arc_apps_;
  // The arc packages installed on the device.
  const base::DictionaryValue* arc_packages_;
  // The Chrome extension registry.
  extensions::ExtensionRegistry* registry_;
  // Used to prevent overwriting of parameters that are set for tests.
  bool testing_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<AppLaunchEventLogger> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchEventLogger);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LAUNCH_EVENT_LOGGER_H_
