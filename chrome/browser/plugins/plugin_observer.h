// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/features.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class PluginObserver : public content::WebContentsObserver,
                       public content::WebContentsUserData<PluginObserver> {
 public:
  ~PluginObserver() override;

  // content::WebContentsObserver implementation.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

 private:
  class ComponentObserver;
  class PluginPlaceholderHost;
  friend class content::WebContentsUserData<PluginObserver>;

  explicit PluginObserver(content::WebContents* web_contents);

  // Message handlers:
  void OnBlockedUnauthorizedPlugin(const base::string16& name,
                                   const std::string& identifier);
  void OnBlockedOutdatedPlugin(int placeholder_id,
                               const std::string& identifier);
  void OnBlockedComponentUpdatedPlugin(int placeholder_id,
                                       const std::string& identifier);
  void OnRemovePluginPlaceholderHost(int placeholder_id);
  void RemoveComponentObserver(int placeholder_id);
  void OnShowFlashPermissionBubble();
  void OnCouldNotLoadPlugin(const base::FilePath& plugin_path);

  // Stores all PluginPlaceholderHosts, keyed by their routing ID.
  std::map<int, std::unique_ptr<PluginPlaceholderHost>> plugin_placeholders_;

  // Stores all ComponentObservers, keyed by their routing ID.
  std::map<int, std::unique_ptr<ComponentObserver>> component_observers_;

  base::WeakPtrFactory<PluginObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginObserver);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
