// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/plugin_service.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_host.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#ifndef DISABLE_NACL
#include "native_client/src/trusted/plugin/nacl_entry_points.h"
#endif
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_list.h"

#if defined(OS_MACOSX)
static void NotifyPluginsOfActivation() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  for (ChildProcessHost::Iterator iter(ChildProcessInfo::PLUGIN_PROCESS);
       !iter.Done(); ++iter) {
    PluginProcessHost* plugin = static_cast<PluginProcessHost*>(*iter);
    plugin->OnAppActivation();
  }
}
#endif

// static
bool PluginService::enable_chrome_plugins_ = true;

// static
PluginService* PluginService::GetInstance() {
  return Singleton<PluginService>::get();
}

// static
void PluginService::EnableChromePlugins(bool enable) {
  enable_chrome_plugins_ = enable;
}

PluginService::PluginService()
    : main_message_loop_(MessageLoop::current()),
      resource_dispatcher_host_(NULL),
      ui_locale_(ASCIIToWide(g_browser_process->GetApplicationLocale())) {
  // Have the NPAPI plugin list search for Chrome plugins as well.
  ChromePluginLib::RegisterPluginsWithNPAPI();
  // Load the one specified on the command line as well.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  FilePath path = command_line->GetSwitchValuePath(switches::kLoadPlugin);
  if (!path.empty()) {
    NPAPI::PluginList::Singleton()->AddExtraPluginPath(path);
  }
#ifndef DISABLE_NACL
  if (command_line->HasSwitch(switches::kInternalNaCl))
    RegisterInternalNaClPlugin();
#endif

#if defined(OS_WIN)
  hkcu_key_.Create(
      HKEY_CURRENT_USER, kRegistryMozillaPlugins, KEY_NOTIFY);
  hklm_key_.Create(
      HKEY_LOCAL_MACHINE, kRegistryMozillaPlugins, KEY_NOTIFY);
  if (hkcu_key_.StartWatching()) {
    hkcu_event_.reset(new base::WaitableEvent(hkcu_key_.watch_event()));
    hkcu_watcher_.StartWatching(hkcu_event_.get(), this);
  }

  if (hklm_key_.StartWatching()) {
    hklm_event_.reset(new base::WaitableEvent(hklm_key_.watch_event()));
    hklm_watcher_.StartWatching(hklm_event_.get(), this);
  }
#endif

  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
#if defined(OS_MACOSX)
  // We need to know when the browser comes forward so we can bring modal plugin
  // windows forward too.
  registrar_.Add(this, NotificationType::APP_ACTIVATED,
                 NotificationService::AllSources());
#endif
}

PluginService::~PluginService() {
#if defined(OS_WIN)
  // Release the events since they're owned by RegKey, not WaitableEvent.
  hkcu_watcher_.StopWatching();
  hklm_watcher_.StopWatching();
  hkcu_event_->Release();
  hklm_event_->Release();
#endif
}

void PluginService::LoadChromePlugins(
    ResourceDispatcherHost* resource_dispatcher_host) {
  if (!enable_chrome_plugins_)
    return;

  resource_dispatcher_host_ = resource_dispatcher_host;
  ChromePluginLib::LoadChromePlugins(GetCPBrowserFuncsForBrowser());
}

void PluginService::SetChromePluginDataDir(const FilePath& data_dir) {
  chrome_plugin_data_dir_ = data_dir;
}

const FilePath& PluginService::GetChromePluginDataDir() {
  return chrome_plugin_data_dir_;
}

const std::wstring& PluginService::GetUILocale() {
  return ui_locale_;
}

PluginProcessHost* PluginService::FindPluginProcess(
    const FilePath& plugin_path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (plugin_path.value().empty()) {
    NOTREACHED() << "should only be called if we have a plugin to load";
    return NULL;
  }

  for (ChildProcessHost::Iterator iter(ChildProcessInfo::PLUGIN_PROCESS);
       !iter.Done(); ++iter) {
    PluginProcessHost* plugin = static_cast<PluginProcessHost*>(*iter);
    if (plugin->info().path == plugin_path)
      return plugin;
  }

  return NULL;
}

PluginProcessHost* PluginService::FindOrStartPluginProcess(
    const FilePath& plugin_path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  PluginProcessHost *plugin_host = FindPluginProcess(plugin_path);
  if (plugin_host)
    return plugin_host;

  WebPluginInfo info;
  if (!NPAPI::PluginList::Singleton()->GetPluginInfoByPath(
          plugin_path, &info)) {
    DCHECK(false);
    return NULL;
  }

  // This plugin isn't loaded by any plugin process, so create a new process.
  plugin_host = new PluginProcessHost();
  if (!plugin_host->Init(info, ui_locale_)) {
    DCHECK(false);  // Init is not expected to fail
    delete plugin_host;
    return NULL;
  }

  return plugin_host;
}

void PluginService::OpenChannelToPlugin(
    ResourceMessageFilter* renderer_msg_filter,
    const GURL& url,
    const std::string& mime_type,
    const std::wstring& locale,
    IPC::Message* reply_msg) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // We don't need a policy URL here because that was already checked by a
  // previous call to GetPluginPath.
  GURL policy_url;
  FilePath plugin_path = GetPluginPath(url, policy_url, mime_type, NULL);
  PluginProcessHost* plugin_host = FindOrStartPluginProcess(plugin_path);
  if (plugin_host) {
    plugin_host->OpenChannelToPlugin(renderer_msg_filter, mime_type, reply_msg);
  } else {
    PluginProcessHost::ReplyToRenderer(
        renderer_msg_filter, IPC::ChannelHandle(), WebPluginInfo(), reply_msg);
  }
}

FilePath PluginService::GetPluginPath(const GURL& url,
                                      const GURL& policy_url,
                                      const std::string& mime_type,
                                      std::string* actual_mime_type) {
  bool allow_wildcard = true;
  WebPluginInfo info;
  if (NPAPI::PluginList::Singleton()->GetPluginInfo(
          url, mime_type, allow_wildcard, &info, actual_mime_type) &&
      PluginAllowedForURL(info.path, policy_url)) {
    return info.path;
  }

  return FilePath();
}

static void PurgePluginListCache(bool reload_pages) {
  for (RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->Send(new ViewMsg_PurgePluginListCache(reload_pages));
  }
}

void PluginService::OnWaitableEventSignaled(
    base::WaitableEvent* waitable_event) {
#if defined(OS_WIN)
  if (waitable_event == hkcu_event_.get()) {
    hkcu_key_.StartWatching();
  } else {
    hklm_key_.StartWatching();
  }

  NPAPI::PluginList::Singleton()->ResetPluginsLoaded();
  PurgePluginListCache(true);
#endif  // defined(OS_WIN)
}

void PluginService::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_LOADED: {
      // TODO(mpcomplete): We also need to force a renderer to refresh its
      // cache of the plugin list when we inject user scripts, since it could
      // have a stale version by the time extensions are loaded.
      // See: http://code.google.com/p/chromium/issues/detail?id=12306
      Extension* extension = Details<Extension>(details).ptr();
      bool plugins_changed = false;
      for (size_t i = 0; i < extension->plugins().size(); ++i) {
        const Extension::PluginInfo& plugin = extension->plugins()[i];
        NPAPI::PluginList::Singleton()->ResetPluginsLoaded();
        NPAPI::PluginList::Singleton()->AddExtraPluginPath(plugin.path);
        plugins_changed = true;
        if (!plugin.is_public)
          private_plugins_[plugin.path] = extension->url();
      }
      if (plugins_changed)
        PurgePluginListCache(false);
      break;
    }

    case NotificationType::EXTENSION_UNLOADED: {
      Extension* extension = Details<Extension>(details).ptr();
      bool plugins_changed = false;
      for (size_t i = 0; i < extension->plugins().size(); ++i) {
        const Extension::PluginInfo& plugin = extension->plugins()[i];
        NPAPI::PluginList::Singleton()->ResetPluginsLoaded();
        NPAPI::PluginList::Singleton()->RemoveExtraPluginPath(plugin.path);
        plugins_changed = true;
        if (!plugin.is_public)
          private_plugins_.erase(plugin.path);
      }
      if (plugins_changed)
        PurgePluginListCache(false);
      break;
    }

#if defined(OS_MACOSX)
    case NotificationType::APP_ACTIVATED: {
      ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
                             NewRunnableFunction(&NotifyPluginsOfActivation));
      break;
    }
#endif

    default:
      DCHECK(false);
  }
}

bool PluginService::PluginAllowedForURL(const FilePath& plugin_path,
                                        const GURL& url) {
  if (url.is_empty())
    return true;  // Caller wants all plugins.

  PrivatePluginMap::iterator it = private_plugins_.find(plugin_path);
  if (it == private_plugins_.end())
    return true;  // This plugin is not private, so it's allowed everywhere.

  // We do a dumb compare of scheme and host, rather than using the domain
  // service, since we only care about this for extensions.
  const GURL& required_url = it->second;
  return (url.scheme() == required_url.scheme() &&
          url.host() == required_url.host());
}
