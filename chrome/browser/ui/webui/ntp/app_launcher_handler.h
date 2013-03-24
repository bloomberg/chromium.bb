// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "sync/api/string_ordinal.h"

class ExtensionEnableFlow;
class ExtensionService;
class PrefChangeRegistrar;
class PrefRegistrySyncable;
class Profile;

// The handler for Javascript messages related to the "apps" view.
class AppLauncherHandler : public content::WebUIMessageHandler,
                           public ExtensionUninstallDialog::Delegate,
                           public ExtensionEnableFlowDelegate,
                           public content::NotificationObserver {
 public:
  explicit AppLauncherHandler(ExtensionService* extension_service);
  virtual ~AppLauncherHandler();

  // Populate a dictionary with the information from an extension.
  static void CreateAppInfo(
      const extensions::Extension* extension,
      ExtensionService* service,
      base::DictionaryValue* value);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Populate the given dictionary with all installed app info.
  void FillAppDictionary(base::DictionaryValue* value);

  // Create a dictionary value for the given extension. May return NULL, e.g. if
  // the given extension is not an app. If non-NULL, the caller assumes
  // ownership of the pointer.
  base::DictionaryValue* GetAppInfo(const extensions::Extension* extension);

  // Populate the given dictionary with the web store promo content.
  void FillPromoDictionary(base::DictionaryValue* value);

  // Callback for the "getApps" message.
  void HandleGetApps(const base::ListValue* args);

  // Callback for the "launchApp" message.
  void HandleLaunchApp(const base::ListValue* args);

  // Callback for the "setLaunchType" message.
  void HandleSetLaunchType(const base::ListValue* args);

  // Callback for the "uninstallApp" message.
  void HandleUninstallApp(const base::ListValue* args);

  // Callback for the "createAppShortcut" message.
  void HandleCreateAppShortcut(const base::ListValue* args);

  // Callback for the "reorderApps" message.
  void HandleReorderApps(const base::ListValue* args);

  // Callback for the "setPageIndex" message.
  void HandleSetPageIndex(const base::ListValue* args);

  // Callback for the "saveAppPageName" message.
  void HandleSaveAppPageName(const base::ListValue* args);

  // Callback for the "generateAppForLink" message.
  void HandleGenerateAppForLink(const base::ListValue* args);

  // Callback for the "recordAppLaunchByURL" message. Takes an escaped URL and a
  // launch source (integer), and if the URL represents an app, records the
  // action for UMA.
  void HandleRecordAppLaunchByUrl(const base::ListValue* args);

  // Callback for "closeNotification" message.
  void HandleNotificationClose(const base::ListValue* args);

  // Callback for "setNotificationsDisabled" message.
  void HandleSetNotificationsDisabled(const base::ListValue* args);

  // Register app launcher preferences.
  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  // Records the given type of app launch for UMA.
  static void RecordAppLaunchType(extension_misc::AppLaunchBucket bucket,
                                  extensions::Manifest::Type app_type);

 private:
  struct AppInstallInfo {
    AppInstallInfo();
    ~AppInstallInfo();

    bool is_bookmark_app;
    string16 title;
    GURL app_url;
    syncer::StringOrdinal page_ordinal;
  };

  // Reset some instance flags we use to track the currently uninstalling app.
  void CleanupAfterUninstall();

  // Records a web store launch in the appropriate histograms.
  static void RecordWebStoreLaunch();

  // Records an app launch in the corresponding |bucket| of the app launch
  // histogram if the |escaped_url| corresponds to an installed app.
  static void RecordAppLaunchByUrl(Profile* profile,
                                   std::string escaped_url,
                                   extension_misc::AppLaunchBucket bucket);

  // Prompts the user to re-enable the app for |extension_id|.
  void PromptToEnableApp(const std::string& extension_id);

  // ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

  // ExtensionEnableFlowDelegate:
  virtual void ExtensionEnableFlowFinished() OVERRIDE;
  virtual void ExtensionEnableFlowAborted(bool user_initiated) OVERRIDE;

  // Returns the ExtensionUninstallDialog object for this class, creating it if
  // needed.
  ExtensionUninstallDialog* GetExtensionUninstallDialog();

  // Continuation for installing a bookmark app after favicon lookup.
  void OnFaviconForApp(scoped_ptr<AppInstallInfo> install_info,
                       const history::FaviconImageResult& image_result);

  // Sends |highlight_app_id_| to the js.
  void SetAppToBeHighlighted();

  void OnPreferenceChanged();

  // The apps are represented in the extensions model, which
  // outlives us since it's owned by our containing profile.
  ExtensionService* const extension_service_;

  // We monitor changes to the extension system so that we can reload the apps
  // when necessary.
  content::NotificationRegistrar registrar_;

  // Monitor extension preference changes so that the Web UI can be notified.
  PrefChangeRegistrar pref_change_registrar_;

  // Used to show confirmation UI for uninstalling extensions in incognito mode.
  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;

  // Used to show confirmation UI for enabling extensions.
  scoped_ptr<ExtensionEnableFlow> extension_enable_flow_;

  // The ids of apps to show on the NTP.
  std::set<std::string> visible_apps_;

  // The id of the extension we are prompting the user about (either enable or
  // uninstall).
  std::string extension_id_prompting_;

  // When true, we ignore changes to the underlying data rather than immediately
  // refreshing. This is useful when making many batch updates to avoid flicker.
  bool ignore_changes_;

  // When true, we have attempted to install a bookmark app, and are still
  // waiting to hear about success or failure from the extensions system.
  bool attempted_bookmark_app_install_;

  // True if we have executed HandleGetApps() at least once.
  bool has_loaded_apps_;

  // The ID of the app to be highlighted on the NTP (i.e. shown on the page
  // and pulsed). This is done for new installs. The actual higlighting occurs
  // when the app is added to the page (via getAppsCallback or appAdded).
  std::string highlight_app_id_;

  // Used for favicon loading tasks.
  CancelableTaskTracker cancelable_task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_
