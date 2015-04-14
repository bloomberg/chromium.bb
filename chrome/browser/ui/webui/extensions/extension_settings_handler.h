// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/warning_service.h"

class ExtensionService;
class GURL;

namespace base {
class FilePath;
class ListValue;
}

namespace content {
class WebUIDataSource;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {
class Extension;
class ExtensionPrefs;

// Extension Settings UI handler.
class ExtensionSettingsHandler
    : public content::WebUIMessageHandler,
      public content::NotificationObserver,
      public content::WebContentsObserver,
      public ExtensionManagement::Observer,
      public ExtensionPrefsObserver,
      public WarningService::Observer,
      public base::SupportsWeakPtr<ExtensionSettingsHandler> {
 public:
  ExtensionSettingsHandler();
  ~ExtensionSettingsHandler() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void GetLocalizedValues(content::WebUIDataSource* source);

 private:
  // content::WebContentsObserver implementation.
  void RenderViewDeleted(content::RenderViewHost* render_view_host) override;
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionPrefsObserver implementation.
  void OnExtensionDisableReasonsChanged(const std::string& extension_id,
                                        int disable_reasons) override;

  // ExtensionManagement::Observer implementation.
  void OnExtensionManagementSettingsChanged() override;

  // WarningService::Observer implementation.
  void ExtensionWarningsChanged() override;

  // Helper method that reloads all unpacked extensions.
  void ReloadUnpackedExtensions();

  // Callback for "extensionSettingsLaunch" message.
  void HandleLaunchMessage(const base::ListValue* args);

  // Callback for "extensionSettingsRepair" message.
  void HandleRepairMessage(const base::ListValue* args);

  // Callback for "extensionSettingsOptions" message.
  void HandleOptionsMessage(const base::ListValue* args);

  // Callback for "extensionSettingsAutoupdate" message.
  void HandleAutoUpdateMessage(const base::ListValue* args);

  // Callback for the "extensionSettingsShowPath" message.
  void HandleShowPath(const base::ListValue* args);

  // Callback for the "extensionSettingsRegister" message.
  void HandleRegisterMessage(const base::ListValue* args);

  // Utility for callbacks that get an extension ID as the sole argument.
  // Returns NULL if the extension isn't active.
  const Extension* GetActiveExtension(const base::ListValue* args);

  // Forces a UI update if appropriate after a notification is received.
  void MaybeUpdateAfterNotification();

  // Called when the reinstallation is complete.
  void OnReinstallComplete(bool success,
                           const std::string& error,
                           webstore_install::Result result);

  // Our model.  Outlives us since it's owned by our containing profile.
  ExtensionService* extension_service_;

  // If true, we will ignore notifications in ::Observe(). This is needed
  // to prevent reloading the page when we were the cause of the
  // notification.
  bool ignore_notifications_;

  // The page may be refreshed in response to a RenderViewHost being destroyed,
  // but the iteration over RenderViewHosts will include the host because the
  // notification is sent when it is in the process of being deleted (and before
  // it is removed from the process). Keep a pointer to it so we can exclude
  // it from the active views.
  content::RenderViewHost* deleting_rvh_;
  // Do the same for a deleting RenderWidgetHost ID and RenderProcessHost ID.
  int deleting_rwh_id_;
  int deleting_rph_id_;

  content::NotificationRegistrar registrar_;

  ScopedObserver<WarningService, WarningService::Observer>
      warning_service_observer_;

  // An observer to listen for notable changes in the ExtensionPrefs, like
  // a change in Disable Reasons.
  ScopedObserver<ExtensionPrefs, ExtensionPrefsObserver>
      extension_prefs_observer_;

  ScopedObserver<ExtensionManagement, ExtensionManagement::Observer>
      extension_management_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
