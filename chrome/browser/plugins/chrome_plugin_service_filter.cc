// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_plugin_service_filter.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "webkit/plugins/npapi/plugin_list.h"

using content::BrowserThread;
using content::PluginService;

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
    const webkit::WebPluginInfo& plugin) {
  base::AutoLock auto_lock(lock_);
  ProcessDetails* details = GetOrRegisterProcess(render_process_id);
  OverriddenPlugin overridden_plugin;
  overridden_plugin.render_view_id = render_view_id;
  overridden_plugin.url = url;
  overridden_plugin.plugin = plugin;
  details->overridden_plugins.push_back(overridden_plugin);
}

void ChromePluginServiceFilter::RestrictPluginToProfileAndOrigin(
    const base::FilePath& plugin_path,
    Profile* profile,
    const GURL& origin) {
  base::AutoLock auto_lock(lock_);
  restricted_plugins_[plugin_path] =
      std::make_pair(PluginPrefs::GetForProfile(profile),
                     origin);
}

void ChromePluginServiceFilter::UnrestrictPlugin(
    const base::FilePath& plugin_path) {
  base::AutoLock auto_lock(lock_);
  restricted_plugins_.erase(plugin_path);
}

bool ChromePluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    int render_view_id,
    const void* context,
    const GURL& url,
    const GURL& policy_url,
    webkit::WebPluginInfo* plugin) {
  base::AutoLock auto_lock(lock_);
  const ProcessDetails* details = GetProcess(render_process_id);

  // Check whether the plugin is overridden.
  if (details) {
    for (size_t i = 0; i < details->overridden_plugins.size(); ++i) {
      if (details->overridden_plugins[i].render_view_id == render_view_id &&
          (details->overridden_plugins[i].url == url ||
           details->overridden_plugins[i].url.is_empty())) {

        bool use = details->overridden_plugins[i].plugin.path == plugin->path;
        if (use)
          *plugin = details->overridden_plugins[i].plugin;
        return use;
      }
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

bool ChromePluginServiceFilter::CanLoadPlugin(int render_process_id,
                                              const base::FilePath& path) {
  // The browser itself sometimes loads plug-ins to e.g. clear plug-in data.
  // We always grant the browser permission.
  if (!render_process_id)
    return true;

  base::AutoLock auto_lock(lock_);
  const ProcessDetails* details = GetProcess(render_process_id);
  if (!details)
    return false;

  if (details->authorized_plugins.find(path) ==
          details->authorized_plugins.end() &&
      details->authorized_plugins.find(base::FilePath()) ==
          details->authorized_plugins.end()) {
    return false;
  }

  return true;
}

void ChromePluginServiceFilter::AuthorizePlugin(
    int render_process_id,
    const base::FilePath& plugin_path) {
  base::AutoLock auto_lock(lock_);
  ProcessDetails* details = GetOrRegisterProcess(render_process_id);
  details->authorized_plugins.insert(plugin_path);
}

void ChromePluginServiceFilter::AuthorizeAllPlugins(int render_process_id) {
  AuthorizePlugin(render_process_id, base::FilePath());
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
          content::Source<content::RenderProcessHost>(source).ptr()->GetID();

      base::AutoLock auto_lock(lock_);
      plugin_details_.erase(render_process_id);
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

ChromePluginServiceFilter::ProcessDetails*
ChromePluginServiceFilter::GetOrRegisterProcess(
    int render_process_id) {
  return &plugin_details_[render_process_id];
}

const ChromePluginServiceFilter::ProcessDetails*
ChromePluginServiceFilter::GetProcess(
    int render_process_id) const {
  std::map<int, ProcessDetails>::const_iterator it =
      plugin_details_.find(render_process_id);
  if (it == plugin_details_.end())
    return NULL;
  return &it->second;
}

ChromePluginServiceFilter::OverriddenPlugin::OverriddenPlugin()
    : render_view_id(MSG_ROUTING_NONE) {
}

ChromePluginServiceFilter::OverriddenPlugin::~OverriddenPlugin() {
}

ChromePluginServiceFilter::ProcessDetails::ProcessDetails() {
}

ChromePluginServiceFilter::ProcessDetails::~ProcessDetails() {
}

