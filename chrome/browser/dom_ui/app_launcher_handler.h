// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_APP_LAUNCHER_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_APP_LAUNCHER_HANDLER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Extension;
class ExtensionsService;

namespace gfx {
  class Rect;
}

// The handler for Javascript messages related to the "apps" view.
class AppLauncherHandler
    : public DOMMessageHandler,
      public ExtensionInstallUI::Delegate,
      public NotificationObserver {
 public:
  explicit AppLauncherHandler(ExtensionsService* extension_service);
  virtual ~AppLauncherHandler();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                      const NotificationSource& source,
                      const NotificationDetails& details);

  // Populate a dictionary with the information from an extension.
  static void CreateAppInfo(Extension* extension, DictionaryValue* value);

  // Callback for the "getApps" message.
  void HandleGetApps(const ListValue* args);

  // Callback for the "launchApp" message.
  void HandleLaunchApp(const ListValue* args);

  // Callback for the "uninstallApp" message.
  void HandleUninstallApp(const ListValue* args);

 private:
  // ExtensionInstallUI::Delegate implementation, used for receiving
  // notification about uninstall confirmation dialog selections.
  virtual void InstallUIProceed(bool create_app_shortcut);
  virtual void InstallUIAbort();

  // Returns the ExtensionInstallUI object for this class, creating it if
  // needed.
  ExtensionInstallUI* GetExtensionInstallUI();

  // Starts the animation of the app icon.
  void AnimateAppIcon(Extension* extension, const gfx::Rect& rect);

  // The apps are represented in the extensions model.
  scoped_refptr<ExtensionsService> extensions_service_;

  // We monitor changes to the extension system so that we can reload the apps
  // when necessary.
  NotificationRegistrar registrar_;

  // Used to show confirmation UI for uninstalling/enabling extensions in
  // incognito mode.
  scoped_ptr<ExtensionInstallUI> install_ui_;

  // The id of the extension we are prompting the user about.
  std::string extension_id_prompting_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_APP_LAUNCHER_HANDLER_H_
