// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_PLUGIN_SERVICE_FILTER_H_
#define CHROME_BROWSER_CHROME_PLUGIN_SERVICE_FILTER_H_
#pragma once

#include <map>
#include <vector>

#include "base/hash_tables.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "content/browser/plugin_service_filter.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class PluginPrefs;
class Profile;

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
  void OverridePluginForTab(int render_process_id,
                            int render_view_id,
                            const GURL& url,
                            const string16& plugin_name);

  // Restricts the given plugin to the given profile and origin of the given
  // URL.
  void RestrictPluginToProfileAndOrigin(const FilePath& plugin_path,
                                        Profile* profile,
                                        const GURL& url);

  // Lifts a restriction on a plug-in.
  void UnrestrictPlugin(const FilePath& plugin_path);

  // PluginServiceFilter implementation:
  virtual bool ShouldUsePlugin(
      int render_process_id,
      int render_view_id,
      const void* context,
      const GURL& url,
      const GURL& policy_url,
      webkit::WebPluginInfo* plugin) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ChromePluginServiceFilter>;

  struct OverriddenPlugin {
    int render_process_id;
    int render_view_id;
    GURL url;  // If empty, the override applies to all urls in render_view.
    string16 plugin_name;
  };

  ChromePluginServiceFilter();
  virtual ~ChromePluginServiceFilter();

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  base::Lock lock_;  // Guards access to member variables.
  // Map of plugin paths to the origin they are restricted to.
  typedef std::pair<const void*, GURL> RestrictedPluginPair;
  typedef base::hash_map<FilePath, RestrictedPluginPair> RestrictedPluginMap;
  RestrictedPluginMap restricted_plugins_;
  typedef std::map<const void*, scoped_refptr<PluginPrefs> > ResourceContextMap;
  ResourceContextMap resource_context_map_;

  std::vector<OverriddenPlugin> overridden_plugins_;
};

#endif  // CHROME_BROWSER_CHROME_PLUGIN_SERVICE_FILTER_H_
