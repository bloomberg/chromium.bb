// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_CHROME_PLUGIN_SERVICE_FILTER_H_
#define CHROME_BROWSER_PLUGINS_CHROME_PLUGIN_SERVICE_FILTER_H_

#include <map>
#include <set>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/common/webplugininfo.h"
#include "url/gurl.h"

class PluginPrefs;
class Profile;

namespace content {
class WebContents;
}

// This class must be created (by calling the |GetInstance| method) on the UI
// thread, but is safe to use on any thread after that.
class ChromePluginServiceFilter : public content::PluginServiceFilter,
                                  public content::NotificationObserver {
 public:
  static ChromePluginServiceFilter* GetInstance();

  // This method should be called on the UI thread.
  void RegisterResourceContext(PluginPrefs* plugin_prefs, const void* context);

  void UnregisterResourceContext(const void* context);

  // Overrides the plugin lookup mechanism for a given tab and object URL to use
  // a specifc plugin.
  void OverridePluginForFrame(int render_process_id,
                              int render_frame_id,
                              const GURL& url,
                              const content::WebPluginInfo& plugin);

  // Restricts the given plugin to the given profile and origin of the given
  // URL.
  void RestrictPluginToProfileAndOrigin(const base::FilePath& plugin_path,
                                        Profile* profile,
                                        const GURL& url);

  // Lifts a restriction on a plug-in.
  void UnrestrictPlugin(const base::FilePath& plugin_path);

  // Authorizes a given plug-in for a given process.
  void AuthorizePlugin(int render_process_id,
                       const base::FilePath& plugin_path);

  // Authorizes all plug-ins for a given WebContents. If |load_blocked| is true,
  // then the renderer is told to load the plugin with given |identifier| (or
  // pllugins if |identifier| is empty).
  // This method can only be called on the UI thread.
  void AuthorizeAllPlugins(content::WebContents* web_contents,
                           bool load_blocked,
                           const std::string& identifier);

  // Returns whether the plugin is found in restricted_plugins_.
  bool IsPluginRestricted(const base::FilePath& plugin_path);

  // PluginServiceFilter implementation:
  virtual bool IsPluginAvailable(
      int render_process_id,
      int render_frame_id,
      const void* context,
      const GURL& url,
      const GURL& policy_url,
      content::WebPluginInfo* plugin) OVERRIDE;

  // CanLoadPlugin always grants permission to the browser
  // (render_process_id == 0)
  virtual bool CanLoadPlugin(
      int render_process_id,
      const base::FilePath& path) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ChromePluginServiceFilter>;

  struct OverriddenPlugin {
    OverriddenPlugin();
    ~OverriddenPlugin();

    int render_frame_id;
    GURL url;  // If empty, the override applies to all urls in render_frame.
    content::WebPluginInfo plugin;
  };

  struct ProcessDetails {
    ProcessDetails();
    ~ProcessDetails();

    std::vector<OverriddenPlugin> overridden_plugins;
    std::set<base::FilePath> authorized_plugins;
  };

  ChromePluginServiceFilter();
  virtual ~ChromePluginServiceFilter();

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  ProcessDetails* GetOrRegisterProcess(int render_process_id);
  const ProcessDetails* GetProcess(int render_process_id) const;

  content::NotificationRegistrar registrar_;

  base::Lock lock_;  // Guards access to member variables.
  // Map of plugin paths to the origin they are restricted to.
  typedef std::pair<const void*, GURL> RestrictedPluginPair;
  typedef base::hash_map<base::FilePath,
                         RestrictedPluginPair> RestrictedPluginMap;
  RestrictedPluginMap restricted_plugins_;
  typedef std::map<const void*, scoped_refptr<PluginPrefs> > ResourceContextMap;
  ResourceContextMap resource_context_map_;

  std::map<int, ProcessDetails> plugin_details_;
};

#endif  // CHROME_BROWSER_PLUGINS_CHROME_PLUGIN_SERVICE_FILTER_H_
