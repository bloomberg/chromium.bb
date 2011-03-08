// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_HANDLER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/browser/webui/web_ui.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ExtensionPrefs;
class ExtensionService;
class NotificationRegistrar;
class PrefChangeRegistrar;
class Profile;

namespace gfx {
  class Rect;
}

// The handler for Javascript messages related to the "apps" view.
class AppLauncherHandler
    : public WebUIMessageHandler,
      public ExtensionInstallUI::Delegate,
      public NotificationObserver {
 public:
  explicit AppLauncherHandler(ExtensionService* extension_service);
  virtual ~AppLauncherHandler();

  // Populate a dictionary with the information from an extension.
  static void CreateAppInfo(const Extension* extension,
                            ExtensionPrefs* extension_prefs,
                            DictionaryValue* value);

  // Callback for pings related to launching apps on the NTP.
  static bool HandlePing(Profile* profile, const std::string& path);

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                      const NotificationSource& source,
                      const NotificationDetails& details);

  // Populate the given dictionary with all installed app info.
  void FillAppDictionary(DictionaryValue* value);

  // Callback for the "getApps" message.
  void HandleGetApps(const ListValue* args);

  // Callback for the "launchApp" message.
  void HandleLaunchApp(const ListValue* args);

  // Callback for the "setLaunchType" message.
  void HandleSetLaunchType(const ListValue* args);

  // Callback for the "uninstallApp" message.
  void HandleUninstallApp(const ListValue* args);

  // Callback for the "hideAppPromo" message.
  void HandleHideAppsPromo(const ListValue* args);

  // Callback for the "createAppShortcut" message.
  void HandleCreateAppShortcut(const ListValue* args);

  // Callback for the "reorderApps" message.
  void HandleReorderApps(const ListValue* args);

  // Callback for the "setPageIndex" message.
  void HandleSetPageIndex(const ListValue* args);

 private:
  // Records a web store launch in the appropriate histograms. |promo_active|
  // specifies if the web store promotion was active.
  static void RecordWebStoreLaunch(bool promo_active);

  // Records an app launch in the corresponding |bucket| of the app launch
  // histogram. |promo_active| specifies if the web store promotion was active.
  static void RecordAppLaunchByID(bool promo_active,
                                  extension_misc::AppLaunchBucket bucket);

  // Records an app launch in the corresponding |bucket| of the app launch
  // histogram if the |escaped_url| corresponds to an installed app.
  static void RecordAppLaunchByURL(Profile* profile,
                                   std::string escaped_url,
                                   extension_misc::AppLaunchBucket bucket);

  // Prompts the user to re-enable the app for |extension_id|.
  void PromptToEnableApp(std::string extension_id);

  // ExtensionInstallUI::Delegate implementation, used for receiving
  // notification about uninstall confirmation dialog selections.
  virtual void InstallUIProceed();
  virtual void InstallUIAbort();

  // Returns the ExtensionInstallUI object for this class, creating it if
  // needed.
  ExtensionInstallUI* GetExtensionInstallUI();

  // Helper that uninstalls all the default apps.
  void UninstallDefaultApps();

  // The apps are represented in the extensions model.
  scoped_refptr<ExtensionService> extensions_service_;

  // We monitor changes to the extension system so that we can reload the apps
  // when necessary.
  NotificationRegistrar registrar_;

  // Monitor extension preference changes so that the Web UI can be notified.
  PrefChangeRegistrar pref_change_registrar_;

  // Used to show confirmation UI for uninstalling/enabling extensions in
  // incognito mode.
  scoped_ptr<ExtensionInstallUI> install_ui_;

  // The id of the extension we are prompting the user about.
  std::string extension_id_prompting_;

  // The type of prompt we are showing the user.
  ExtensionInstallUI::PromptType extension_prompt_type_;

  // Whether the promo is currently being shown.
  bool promo_active_;

  // When true, we ignore changes to the underlying data rather than immediately
  // refreshing. This is useful when making many batch updates to avoid flicker.
  bool ignore_changes_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_LAUNCHER_HANDLER_H_
