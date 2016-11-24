// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PLUGINS_PLUGINS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PLUGINS_PLUGINS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/mojo_web_ui_handler.h"
#include "chrome/browser/ui/webui/plugins/plugins.mojom.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/webplugininfo.h"
#include "mojo/public/cpp/bindings/binding.h"

class PluginsPageHandler : public MojoWebUIHandler,
                           public mojom::PluginsPageHandler,
                           public content::NotificationObserver {
 public:
  PluginsPageHandler(content::WebUI* web_ui,
                     mojo::InterfaceRequest<mojom::PluginsPageHandler> request);
  ~PluginsPageHandler() override;

  // mojom::PluginsPageHandler overrides:
  void GetPluginsData(const GetPluginsDataCallback& callback) override;
  void GetShowDetails(const GetShowDetailsCallback& callback) override;
  void SaveShowDetailsToPrefs(bool details_mode) override;
  void SetPluginAlwaysAllowed(const std::string& plugin, bool allowed) override;
  void SetClientPage(mojom::PluginsPagePtr page) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  std::vector<mojom::PluginDataPtr> GeneratePluginsData(
      const std::vector<content::WebPluginInfo>& plugins);

  mojom::PluginFilePtr GeneratePluginFile(const content::WebPluginInfo& plugin,
                                          const base::string16& group_name,
                                          bool plugin_enabled) const;

  // Detect a plugin's enabled mode (one of enabledByUser, disabledByUser,
  // enabledByPolicy, disabledByPolicy).
  std::string GetPluginEnabledMode(const base::string16& plugin_name,
                                   const base::string16& group_name,
                                   bool plugin_enabled) const;

  // Returns whether the policy for the current profile is "Click To Play" i.e.
  // DefaultPluginsSetting is 3 (ASK).
  bool GetClickToPlayPolicyEnabled() const;

  // Detect a plugin group's enabled mode (one of enabledByUser, disabledByUser,
  // enabledByPolicy, disabledByPolicy, managedByPolicy).
  std::string GetPluginGroupEnabledMode(
      const std::vector<mojom::PluginFilePtr>& plugin_files,
      bool group_enabled) const;

  // Called on the UI thread when the plugin info requested fom GetPluginsData
  // is ready.
  void RespondWithPluginsData(
      const GetPluginsDataCallback& callback,
      const std::vector<content::WebPluginInfo>& plugins);

  // Called on the UI thread when the plugin info has changed and calls the page
  // to update it.
  void NotifyWithPluginsData(
      const std::vector<content::WebPluginInfo>& plugins);

  content::NotificationRegistrar registrar_;

  // This pref guards the value whether about:plugins is in the details mode or
  // not.
  BooleanPrefMember show_details_;

  // Owned by RenderFrameHostImpl.
  content::WebUI* web_ui_;

  mojo::Binding<mojom::PluginsPageHandler> binding_;

  // Handle back to the page by which JS methods can be called.
  mojom::PluginsPagePtr page_;

  base::WeakPtrFactory<PluginsPageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PLUGINS_PLUGINS_HANDLER_H_
