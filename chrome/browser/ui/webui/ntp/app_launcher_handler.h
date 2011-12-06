// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/browser/cancelable_request.h"
#include "content/browser/webui/web_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class AppNotification;
class ExtensionService;
class PrefChangeRegistrar;
class Profile;

// The handler for Javascript messages related to the "apps" view.
class AppLauncherHandler : public WebUIMessageHandler,
                           public ExtensionUninstallDialog::Delegate,
                           public ExtensionInstallUI::Delegate,
                           public content::NotificationObserver {
 public:
  explicit AppLauncherHandler(ExtensionService* extension_service);
  virtual ~AppLauncherHandler();

  // Whether the app should be excluded from the "apps" list because
  // it is special (such as the Web Store app).
  static bool IsAppExcludedFromList(const Extension* extension);

  // Populate a dictionary with the information from an extension.
  static void CreateAppInfo(
      const Extension* extension,
      const AppNotification* notification,
      ExtensionService* service,
      base::DictionaryValue* value);

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
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
  base::DictionaryValue* GetAppInfo(const Extension* extension);

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

  // Callback for the "hideAppPromo" message.
  void HandleHideAppsPromo(const base::ListValue* args);

  // Callback for the "createAppShortcut" message.
  void HandleCreateAppShortcut(const base::ListValue* args);

  // Callback for the "reorderApps" message.
  void HandleReorderApps(const base::ListValue* args);

  // Callback for the "setPageIndex" message.
  void HandleSetPageIndex(const base::ListValue* args);

  // Callback for the "promoSeen" message.
  void HandlePromoSeen(const base::ListValue* args);

  // Callback for the "saveAppPageName" message.
  void HandleSaveAppPageName(const base::ListValue* args);

  // Callback for the "generateAppForLink" message.
  void HandleGenerateAppForLink(const base::ListValue* args);

  // Callback for the "recordAppLaunchByURL" message. Takes an escaped URL and a
  // launch source (integer), and if the URL represents an app, records the
  // action for UMA.
  void HandleRecordAppLaunchByURL(const base::ListValue* args);

  // Callback for "closeNotification" message.
  void HandleNotificationClose(const base::ListValue* args);

  // Callback for "setNotificationsDisabled" message.
  void HandleSetNotificationsDisabled(const base::ListValue* args);

  // Register app launcher preferences.
  static void RegisterUserPrefs(PrefService* pref_service);

 private:
  struct AppInstallInfo {
    bool is_bookmark_app;
    string16 title;
    GURL app_url;
    int page_index;
  };

  // Records a web store launch in the appropriate histograms. |promo_active|
  // specifies if the web store promotion was active.
  static void RecordWebStoreLaunch(bool promo_active);

  // Records an app launch in the corresponding |bucket| of the app launch
  // histogram. |promo_active| specifies if the web store promotion was active.
  static void RecordAppLaunchByID(extension_misc::AppLaunchBucket bucket);

  // Records an app launch in the corresponding |bucket| of the app launch
  // histogram if the |escaped_url| corresponds to an installed app.
  static void RecordAppLaunchByURL(Profile* profile,
                                   std::string escaped_url,
                                   extension_misc::AppLaunchBucket bucket);

  // Prompts the user to re-enable the app for |extension_id|.
  void PromptToEnableApp(const std::string& extension_id);

  // ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

  // ExtensionInstallUI::Delegate:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // Returns the ExtensionUninstallDialog object for this class, creating it if
  // needed.
  ExtensionUninstallDialog* GetExtensionUninstallDialog();

  // Returns the ExtensionInstallUI object for this class, creating it if
  // needed.
  ExtensionInstallUI* GetExtensionInstallUI();

  // Helper that uninstalls all the default apps.
  void UninstallDefaultApps();

  // Continuation for installing a bookmark app after favicon lookup.
  void OnFaviconForApp(FaviconService::Handle handle,
                       history::FaviconData data);

  // Sends |highlight_app_id_| to the js.
  void SetAppToBeHighlighted();

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

  // Used to show confirmation UI for enabling extensions in incognito mode.
  scoped_ptr<ExtensionInstallUI> extension_install_ui_;

  // The id of the extension we are prompting the user about.
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

  // Hold state for favicon requests.
  CancelableRequestConsumerTSimple<AppInstallInfo*> favicon_consumer_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_APP_LAUNCHER_HANDLER_H_
