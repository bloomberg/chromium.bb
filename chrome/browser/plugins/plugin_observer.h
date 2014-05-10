// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include <map>
#endif

class GURL;
class PluginFinder;
class PluginMetadata;

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginInstaller;
class PluginPlaceholderHost;
#endif

namespace content {
class WebContents;
}

namespace infobars {
class InfoBarDelegate;
}

class PluginObserver : public content::WebContentsObserver,
                       public content::WebContentsUserData<PluginObserver> {
 public:
  virtual ~PluginObserver();

  // content::WebContentsObserver implementation.
  virtual void RenderFrameCreated(
      content::RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void PluginCrashed(const base::FilePath& plugin_path,
                             base::ProcessId plugin_pid) OVERRIDE;
  virtual bool OnMessageReceived(
      const IPC::Message& message,
      content::RenderFrameHost* render_frame_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  explicit PluginObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PluginObserver>;

  class PluginPlaceholderHost;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  void InstallMissingPlugin(PluginInstaller* installer,
                            const PluginMetadata* plugin_metadata);
#endif

  // Message handlers:
  void OnBlockedUnauthorizedPlugin(const base::string16& name,
                                   const std::string& identifier);
  void OnBlockedOutdatedPlugin(int placeholder_id,
                               const std::string& identifier);
#if defined(ENABLE_PLUGIN_INSTALLATION)
  void OnFindMissingPlugin(int placeholder_id, const std::string& mime_type);

  void OnRemovePluginPlaceholderHost(int placeholder_id);
#endif
  void OnOpenAboutPlugins();
  void OnCouldNotLoadPlugin(const base::FilePath& plugin_path);
  void OnNPAPINotSupported(const std::string& identifier);

#if defined(ENABLE_PLUGIN_INSTALLATION)
  // Stores all PluginPlaceholderHosts, keyed by their routing ID.
  std::map<int, PluginPlaceholderHost*> plugin_placeholders_;
#endif

  base::WeakPtrFactory<PluginObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginObserver);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
