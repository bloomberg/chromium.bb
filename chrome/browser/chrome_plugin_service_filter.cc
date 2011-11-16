// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_plugin_service_filter.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/plugin_service.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/resource_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"

using content::BrowserThread;
using webkit::npapi::PluginGroup;

// static
ChromePluginServiceFilter* ChromePluginServiceFilter::GetInstance() {
  return Singleton<ChromePluginServiceFilter>::get();
}

void ChromePluginServiceFilter::RegisterResourceContext(
    PluginPrefs* plugin_prefs,
    const void* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  resource_context_map_[context] = plugin_prefs;
}

void ChromePluginServiceFilter::UnregisterResourceContext(
    const void* context) {
  base::AutoLock lock(lock_);
  resource_context_map_.erase(context);
}

void ChromePluginServiceFilter::OverridePluginForTab(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const string16& plugin_name) {
  OverriddenPlugin overridden_plugin;
  overridden_plugin.render_process_id = render_process_id;
  overridden_plugin.render_view_id = render_view_id;
  overridden_plugin.url = url;
  overridden_plugin.plugin_name = plugin_name;
  base::AutoLock auto_lock(lock_);
  overridden_plugins_.push_back(overridden_plugin);
}

void ChromePluginServiceFilter::RestrictPluginToProfileAndOrigin(
    const FilePath& plugin_path,
    Profile* profile,
    const GURL& origin) {
  base::AutoLock auto_lock(lock_);
  restricted_plugins_[plugin_path] =
      std::make_pair(PluginPrefs::GetForProfile(profile),
                     origin);
}

void ChromePluginServiceFilter::UnrestrictPlugin(
    const FilePath& plugin_path) {
  base::AutoLock auto_lock(lock_);
  restricted_plugins_.erase(plugin_path);
}

bool ChromePluginServiceFilter::ShouldUsePlugin(
    int render_process_id,
    int render_view_id,
    const void* context,
    const GURL& url,
    const GURL& policy_url,
    webkit::WebPluginInfo* plugin) {
  base::AutoLock auto_lock(lock_);
  // Check whether the plugin is overridden.
  for (size_t i = 0; i < overridden_plugins_.size(); ++i) {
    if (overridden_plugins_[i].render_process_id == render_process_id &&
        overridden_plugins_[i].render_view_id == render_view_id &&
        (overridden_plugins_[i].url == url ||
         overridden_plugins_[i].url.is_empty())) {

      bool use = overridden_plugins_[i].plugin_name == plugin->name;
      if (use &&
          plugin->name == ASCIIToUTF16(PluginGroup::kAdobeReaderGroupName)) {
        // If the caller is forcing the Adobe Reader plugin, then don't show the
        // blocked plugin UI if it's vulnerable.
        plugin->version = ASCIIToUTF16("11.0.0.0");
      }
      return use;
    }
  }

  // Check whether the plugin is disabled.
  ResourceContextMap::iterator prefs_it =
      resource_context_map_.find(context);
  if (prefs_it == resource_context_map_.end())
    return false;

  PluginPrefs* plugin_prefs = prefs_it->second.get();
  if (!plugin_prefs->IsPluginEnabled(*plugin))
    return false;

  // Check whether the plugin is restricted to a URL.
  RestrictedPluginMap::const_iterator it =
      restricted_plugins_.find(plugin->path);
  if (it != restricted_plugins_.end()) {
    if (it->second.first != plugin_prefs)
      return false;
    const GURL& origin = it->second.second;
    if (!origin.is_empty() &&
        (policy_url.scheme() != origin.scheme() ||
         policy_url.host() != origin.host() ||
         policy_url.port() != origin.port())) {
      return false;
    }
  }

  return true;
}

ChromePluginServiceFilter::ChromePluginServiceFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
                 content::NotificationService::AllSources());
}

ChromePluginServiceFilter::~ChromePluginServiceFilter() {
}

void ChromePluginServiceFilter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      int render_process_id =
          content::Source<RenderProcessHost>(source).ptr()->id();

      base::AutoLock auto_lock(lock_);
      for (size_t i = 0; i < overridden_plugins_.size(); ++i) {
        if (overridden_plugins_[i].render_process_id == render_process_id) {
          overridden_plugins_.erase(overridden_plugins_.begin() + i);
          break;
        }
      }
      break;
    }
    case chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      PluginService::GetInstance()->PurgePluginListCache(profile, false);
      if (profile && profile->HasOffTheRecordProfile()) {
        PluginService::GetInstance()->PurgePluginListCache(
            profile->GetOffTheRecordProfile(), false);
      }
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}
