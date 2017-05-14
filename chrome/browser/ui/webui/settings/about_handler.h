// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ABOUT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ABOUT_HANDLER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
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

namespace content {
class WebUIDataSource;
}

class Profile;

namespace settings {

// WebUI message handler for the help page.
class AboutHandler : public settings::SettingsPageUIHandler,
                     public content::NotificationObserver {
 public:
  AboutHandler();
  ~AboutHandler() override;

  static AboutHandler* Create(content::WebUIDataSource* html_source,
                              Profile* profile);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Returns the browser version as a string.
  static base::string16 BuildBrowserVersionString();

 private:
  void OnDeviceAutoUpdatePolicyChanged(const base::Value* previous_policy,
                                       const base::Value* current_policy);

  // Called once the JS page is ready to be called, serves as a signal to the
  // handler to register C++ observers.
  void HandlePageReady(const base::ListValue* args);

  // Called once when the page has loaded. On ChromeOS, this gets the current
  // update status. On other platforms, it will request and perform an update
  // (if one is available).
  void HandleRefreshUpdateStatus(const base::ListValue* args);
  void RefreshUpdateStatus();

#if defined(OS_MACOSX)
  // Promotes the updater for all users.
  void PromoteUpdater(const base::ListValue* args);
#endif

  // Opens the feedback dialog. |args| must be empty.
  void HandleOpenFeedbackDialog(const base::ListValue* args);

  // Opens the help page. |args| must be empty.
  void HandleOpenHelpPage(const base::ListValue* args);

#if defined(OS_CHROMEOS)
  // Sets the release track version.
  void HandleSetChannel(const base::ListValue* args);

  // Retrieves OS, ARC and firmware versions.
  void HandleGetVersionInfo(const base::ListValue* args);
  void OnGetVersionInfoReady(
      std::string callback_id,
      std::unique_ptr<base::DictionaryValue> version_info);

  // Retrieves combined channel info.
  void HandleGetChannelInfo(const base::ListValue* args);
  // Callbacks for version_updater_->GetChannel calls.
  void OnGetCurrentChannel(std::string callback_id,
                           const std::string& current_channel);
  void OnGetTargetChannel(std::string callback_id,
                          const std::string& current_channel,
                          const std::string& target_channel);

  // Checks for and applies update, triggered by JS.
  void HandleRequestUpdate(const base::ListValue* args);

  // Checks for and applies update over cellular connection, triggered by JS.
  // Target version and size should be included in the list of arguments.
  void HandleRequestUpdateOverCellular(const base::ListValue* args);

  // Checks for and applies update over cellular connection to the given target.
  void RequestUpdateOverCellular(const std::string& target_version,
                                 int64_t target_size);

#endif

  // Checks for and applies update.
  void RequestUpdate();

  // Callback method which forwards status updates to the page.
  void SetUpdateStatus(VersionUpdater::Status status,
                       int progress,
                       const std::string& version,
                       int64_t size,
                       const base::string16& fail_message);

#if defined(OS_MACOSX)
  // Callback method which forwards promotion state to the page.
  void SetPromotionState(VersionUpdater::PromotionState state);
#endif

#if defined(OS_CHROMEOS)
  void HandleGetRegulatoryInfo(const base::ListValue* args);

  // Callback for when the directory with the regulatory label image and alt
  // text has been found.
  void OnRegulatoryLabelDirFound(std::string callback_id,
                                 const base::FilePath& label_dir_path);

  // Callback for when the regulatory text has been read.
  void OnRegulatoryLabelTextRead(std::string callback_id,
                                 const base::FilePath& label_dir_path,
                                 const std::string& text);
#endif

  // Specialized instance of the VersionUpdater used to update the browser.
  std::unique_ptr<VersionUpdater> version_updater_;

  // Used to observe notifications.
  content::NotificationRegistrar registrar_;

  // Used to observe changes in the |kDeviceAutoUpdateDisabled| policy.
  std::unique_ptr<policy::PolicyChangeRegistrar> policy_registrar_;

  // Used for callbacks.
  base::WeakPtrFactory<AboutHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AboutHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ABOUT_HANDLER_H_
