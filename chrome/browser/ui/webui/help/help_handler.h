// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_HELP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_HELP_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "components/policy/core/common/policy_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

#if defined(OS_CHROMEOS)
#include "base/task/cancelable_task_tracker.h"
#include "chromeos/system/version_loader.h"
#endif  // defined(OS_CHROMEOS)

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

// WebUI message handler for the help page.
class HelpHandler : public content::WebUIMessageHandler,
                    public content::NotificationObserver {
 public:
  HelpHandler();
  ~HelpHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Adds string values for the UI to |localized_strings|.
  static void GetLocalizedValues(base::DictionaryValue* localized_strings);

  // NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Returns the browser version as a string.
  static base::string16 BuildBrowserVersionString();

 private:
  void OnDeviceAutoUpdatePolicyChanged(const base::Value* previous_policy,
                                       const base::Value* current_policy);

  // On ChromeOS, this gets the current update status. On other platforms, it
  // will request and perform an update (if one is available).
  void RefreshUpdateStatus();

  // Initializes querying values for the page.
  void OnPageLoaded(const base::ListValue* args);

#if defined(OS_MACOSX)
  // Promotes the updater for all users.
  void PromoteUpdater(const base::ListValue* args);
#endif

  // Relaunches the browser. |args| must be empty.
  void RelaunchNow(const base::ListValue* args);

  // Opens the feedback dialog. |args| must be empty.
  void OpenFeedbackDialog(const base::ListValue* args);

  // Opens the help page. |args| must be empty.
  void OpenHelpPage(const base::ListValue* args);

#if defined(OS_CHROMEOS)
  // Sets the release track version.
  void SetChannel(const base::ListValue* args);

  // Performs relaunch and powerwash.
  void RelaunchAndPowerwash(const base::ListValue* args);
#endif

  // Checks for and applies update.
  void RequestUpdate(const base::ListValue* args);

  // Callback method which forwards status updates to the page.
  void SetUpdateStatus(VersionUpdater::Status status,
                       int progress,
                       const base::string16& fail_message);

#if defined(OS_MACOSX)
  // Callback method which forwards promotion state to the page.
  void SetPromotionState(VersionUpdater::PromotionState state);
#endif

#if defined(OS_CHROMEOS)
  // Callbacks from VersionLoader.
  void OnOSVersion(const std::string& version);
  void OnARCVersion(const std::string& firmware);
  void OnOSFirmware(const std::string& firmware);
  void OnCurrentChannel(const std::string& channel);
  void OnTargetChannel(const std::string& channel);

  // Callback for when the directory with the regulatory label image and alt
  // text has been found.
  void OnRegulatoryLabelDirFound(const base::FilePath& path);

  // Callback for setting the regulatory label source.
  void OnRegulatoryLabelImageFound(const base::FilePath& path);

  // Callback for setting the regulatory label alt text.
  void OnRegulatoryLabelTextRead(const std::string& text);
#endif

  // Specialized instance of the VersionUpdater used to update the browser.
  std::unique_ptr<VersionUpdater> version_updater_;

  // Used to observe notifications.
  content::NotificationRegistrar registrar_;

  // Used to observe changes in the |kDeviceAutoUpdateDisabled| policy.
  policy::PolicyChangeRegistrar policy_registrar_;

  // Used for callbacks.
  base::WeakPtrFactory<HelpHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HelpHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_HELP_HANDLER_H_
