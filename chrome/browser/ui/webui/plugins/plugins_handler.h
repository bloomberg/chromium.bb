// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PLUGINS_PLUGINS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PLUGINS_PLUGINS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/values.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/webplugininfo.h"

class PluginsHandler : public content::WebUIMessageHandler,
                       public content::NotificationObserver {
 public:
  PluginsHandler();
  ~PluginsHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  // Callback for the "requestPluginsData" message.
  void HandleRequestPluginsData(const base::ListValue* args);

  // Callback for the "enablePlugin" message.
  void HandleEnablePluginMessage(const base::ListValue* args);

  // Callback for the "saveShowDetailsToPrefs" message.
  void HandleSaveShowDetailsToPrefs(const base::ListValue* args);

  // Calback for the "getShowDetails" message.
  void HandleGetShowDetails(const base::ListValue* args);

  // Callback for the "setPluginAlwaysAllowed" message.
  void HandleSetPluginAlwaysAllowed(const base::ListValue* args);

  void LoadPlugins();

  // Called on the UI thread when the plugin information is ready.
  void PluginsLoaded(const std::vector<content::WebPluginInfo>& plugins);

  // Detect a plugin's enabled mode (one of enabledByUser, disabledByUser,
  // enabledByPolicy, disabledByPolicy).
  std::string GetPluginEnabledMode(const base::string16& plugin_name,
                                   const base::string16& group_name,
                                   bool plugin_enabled) const;

  // Detect a plugin group's enabled mode (one of enabledByUser, disabledByUser,
  // enabledByPolicy, disabledByPolicy, managedByPolicy).
  std::string GetPluginGroupEnabledMode(const base::ListValue& plugin_files,
                                        bool group_enabled) const;

  content::NotificationRegistrar registrar_;

  // This pref guards the value whether about:plugins is in the details mode or
  // not.
  BooleanPrefMember show_details_;

  base::WeakPtrFactory<PluginsHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginsHandler);
};

#endif // CHROME_BROWSER_UI_WEBUI_PLUGINS_PLUGINS_HANDLER_H_
