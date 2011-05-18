// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service.h"

#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/resource_context.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "content/common/pepper_plugin_registry.h"
#include "content/common/plugin_messages.h"
#include "content/common/view_messages.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugininfo.h"

#if defined(OS_LINUX)
using ::base::files::FilePathWatcher;
#endif

#if defined(OS_MACOSX)
static void NotifyPluginsOfActivation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::PLUGIN_PROCESS);
       !iter.Done(); ++iter) {
    PluginProcessHost* plugin = static_cast<PluginProcessHost*>(*iter);
    plugin->OnAppActivation();
  }
}
#endif

#if defined(OS_LINUX)
// Delegate class for monitoring directories.
class PluginDirWatcherDelegate : public FilePathWatcher::Delegate {
  virtual void OnFilePathChanged(const FilePath& path) OVERRIDE {
    VLOG(1) << "Watched path changed: " << path.value();
    // Make the plugin list update itself
    webkit::npapi::PluginList::Singleton()->RefreshPlugins();
  }
  virtual void OnFilePathError(const FilePath& path) OVERRIDE {
    // TODO(pastarmovj): Add some sensible error handling. Maybe silently
    // stopping the watcher would be enough. Or possibly restart it.
    NOTREACHED();
  }
};
#endif

// static
PluginService* PluginService::GetInstance() {
  return Singleton<PluginService>::get();
}

PluginService::PluginService()
    : ui_locale_(
          content::GetContentClient()->browser()->GetApplicationLocale()) {
  RegisterPepperPlugins();

  // Load any specified on the command line as well.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  FilePath path = command_line->GetSwitchValuePath(switches::kLoadPlugin);
  if (!path.empty())
    webkit::npapi::PluginList::Singleton()->AddExtraPluginPath(path);
  path = command_line->GetSwitchValuePath(switches::kExtraPluginDir);
  if (!path.empty())
    webkit::npapi::PluginList::Singleton()->AddExtraPluginDir(path);

  // Start watching for changes in the plugin list. This means watching
  // for changes in the Windows registry keys and on both Windows and POSIX
  // watch for changes in the paths that are expected to contain plugins.
#if defined(OS_WIN)
  if (hkcu_key_.Create(HKEY_CURRENT_USER,
                       webkit::npapi::kRegistryMozillaPlugins,
                       KEY_NOTIFY) == ERROR_SUCCESS) {
    if (hkcu_key_.StartWatching() == ERROR_SUCCESS) {
      hkcu_event_.reset(new base::WaitableEvent(hkcu_key_.watch_event()));
      hkcu_watcher_.StartWatching(hkcu_event_.get(), this);
    }
  }
  if (hklm_key_.Create(HKEY_LOCAL_MACHINE,
                       webkit::npapi::kRegistryMozillaPlugins,
                       KEY_NOTIFY) == ERROR_SUCCESS) {
    if (hklm_key_.StartWatching() == ERROR_SUCCESS) {
      hklm_event_.reset(new base::WaitableEvent(hklm_key_.watch_event()));
      hklm_watcher_.StartWatching(hklm_event_.get(), this);
    }
  }
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  // Also find plugins in a user-specific plugins dir,
  // e.g. ~/.config/chromium/Plugins.
  FilePath user_data_dir;
  if (PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    webkit::npapi::PluginList::Singleton()->AddExtraPluginDir(
        user_data_dir.Append("Plugins"));
  }
#endif
// The FilePathWatcher produces too many false positives on MacOS (access time
// updates?) which will lead to enforcing updates of the plugins way too often.
// On ChromeOS the user can't install plugins anyway and on Windows all
// important plugins register themselves in the registry so no need to do that.
#if defined(OS_LINUX)
  file_watcher_delegate_ = new PluginDirWatcherDelegate();
  // Get the list of all paths for registering the FilePathWatchers
  // that will track and if needed reload the list of plugins on runtime.
  std::vector<FilePath> plugin_dirs;
  webkit::npapi::PluginList::Singleton()->GetPluginDirectories(
      &plugin_dirs);

  for (size_t i = 0; i < plugin_dirs.size(); ++i) {
    // FilePathWatcher can not handle non-absolute paths under windows.
    // We don't watch for file changes in windows now but if this should ever
    // be extended to Windows these lines might save some time of debugging.
#if defined(OS_WIN)
    if (!plugin_dirs[i].IsAbsolute())
      continue;
#endif
    FilePathWatcher* watcher = new FilePathWatcher();
    VLOG(1) << "Watching for changes in: " << plugin_dirs[i].value();
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(
            &PluginService::RegisterFilePathWatcher,
            watcher, plugin_dirs[i], file_watcher_delegate_));
    file_watchers_.push_back(watcher);
  }
#endif
#if defined(OS_MACOSX)
  // We need to know when the browser comes forward so we can bring modal plugin
  // windows forward too.
  registrar_.Add(this, NotificationType::APP_ACTIVATED,
                 NotificationService::AllSources());
#endif
  registrar_.Add(this, NotificationType::PLUGIN_ENABLE_STATUS_CHANGED,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
}

PluginService::~PluginService() {
#if defined(OS_WIN)
  // Release the events since they're owned by RegKey, not WaitableEvent.
  hkcu_watcher_.StopWatching();
  hklm_watcher_.StopWatching();
  if (hkcu_event_.get())
    hkcu_event_->Release();
  if (hklm_event_.get())
    hklm_event_->Release();
#endif
}

const std::string& PluginService::GetUILocale() {
  return ui_locale_;
}

PluginProcessHost* PluginService::FindNpapiPluginProcess(
    const FilePath& plugin_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::PLUGIN_PROCESS);
       !iter.Done(); ++iter) {
    PluginProcessHost* plugin = static_cast<PluginProcessHost*>(*iter);
    if (plugin->info().path == plugin_path)
      return plugin;
  }

  return NULL;
}

PpapiPluginProcessHost* PluginService::FindPpapiPluginProcess(
    const FilePath& plugin_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (BrowserChildProcessHost::Iterator iter(
           ChildProcessInfo::PPAPI_PLUGIN_PROCESS);
       !iter.Done(); ++iter) {
    PpapiPluginProcessHost* plugin =
        static_cast<PpapiPluginProcessHost*>(*iter);
    if (plugin->plugin_path() == plugin_path)
      return plugin;
  }

  return NULL;
}

PpapiBrokerProcessHost* PluginService::FindPpapiBrokerProcess(
    const FilePath& broker_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (BrowserChildProcessHost::Iterator iter(
           ChildProcessInfo::PPAPI_BROKER_PROCESS);
       !iter.Done(); ++iter) {
    PpapiBrokerProcessHost* broker =
        static_cast<PpapiBrokerProcessHost*>(*iter);
    if (broker->broker_path() == broker_path)
      return broker;
  }

  return NULL;
}

PluginProcessHost* PluginService::FindOrStartNpapiPluginProcess(
    const FilePath& plugin_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  PluginProcessHost* plugin_host = FindNpapiPluginProcess(plugin_path);
  if (plugin_host)
    return plugin_host;

  webkit::npapi::WebPluginInfo info;
  if (!webkit::npapi::PluginList::Singleton()->GetPluginInfoByPath(
          plugin_path, &info)) {
    return NULL;
  }

  // This plugin isn't loaded by any plugin process, so create a new process.
  scoped_ptr<PluginProcessHost> new_host(new PluginProcessHost());
  if (!new_host->Init(info, ui_locale_)) {
    NOTREACHED();  // Init is not expected to fail.
    return NULL;
  }
  return new_host.release();
}

PpapiPluginProcessHost* PluginService::FindOrStartPpapiPluginProcess(
    const FilePath& plugin_path,
    PpapiPluginProcessHost::Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  PpapiPluginProcessHost* plugin_host = FindPpapiPluginProcess(plugin_path);
  if (plugin_host)
    return plugin_host;

  // Validate that the plugin is actually registered.
  PepperPluginInfo* info = GetRegisteredPpapiPluginInfo(plugin_path);
  if (!info)
    return NULL;

  // This plugin isn't loaded by any plugin process, so create a new process.
  scoped_ptr<PpapiPluginProcessHost> new_host(new PpapiPluginProcessHost(
      client->GetResourceContext()->host_resolver()));
  if (!new_host->Init(*info)) {
    NOTREACHED();  // Init is not expected to fail.
    return NULL;
  }
  return new_host.release();
}

PpapiBrokerProcessHost* PluginService::FindOrStartPpapiBrokerProcess(
    const FilePath& plugin_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  PpapiBrokerProcessHost* plugin_host = FindPpapiBrokerProcess(plugin_path);
  if (plugin_host)
    return plugin_host;

  // Validate that the plugin is actually registered.
  PepperPluginInfo* info = GetRegisteredPpapiPluginInfo(plugin_path);
  if (!info)
    return NULL;

  // TODO(ddorwin): Uncomment once out of process is supported.
  // DCHECK(info->is_out_of_process);

  // This broker isn't loaded by any broker process, so create a new process.
  scoped_ptr<PpapiBrokerProcessHost> new_host(
      new PpapiBrokerProcessHost);
  if (!new_host->Init(*info)) {
    NOTREACHED();  // Init is not expected to fail.
    return NULL;
  }
  return new_host.release();
}

void PluginService::OpenChannelToNpapiPlugin(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const std::string& mime_type,
    PluginProcessHost::Client* client) {
  // The PluginList::GetPluginInfo may need to load the plugins.  Don't do it on
  // the IO thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &PluginService::GetAllowedPluginForOpenChannelToPlugin,
          render_process_id, render_view_id, url, mime_type, client));
}

void PluginService::OpenChannelToPpapiPlugin(
    const FilePath& path,
    PpapiPluginProcessHost::Client* client) {
  PpapiPluginProcessHost* plugin_host = FindOrStartPpapiPluginProcess(
      path, client);
  if (plugin_host)
    plugin_host->OpenChannelToPlugin(client);
  else  // Send error.
    client->OnChannelOpened(base::kNullProcessHandle, IPC::ChannelHandle());
}

void PluginService::OpenChannelToPpapiBroker(
    const FilePath& path,
    PpapiBrokerProcessHost::Client* client) {
  PpapiBrokerProcessHost* plugin_host = FindOrStartPpapiBrokerProcess(path);
  if (plugin_host)
    plugin_host->OpenChannelToPpapiBroker(client);
  else  // Send error.
    client->OnChannelOpened(base::kNullProcessHandle, IPC::ChannelHandle());
}

void PluginService::GetAllowedPluginForOpenChannelToPlugin(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const std::string& mime_type,
    PluginProcessHost::Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  webkit::npapi::WebPluginInfo info;
  bool found = GetPluginInfo(
      render_process_id, render_view_id, url, mime_type, &info, NULL);
  FilePath plugin_path;
  if (found && webkit::npapi::IsPluginEnabled(info))
    plugin_path = FilePath(info.path);

  // Now we jump back to the IO thread to finish opening the channel.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &PluginService::FinishOpenChannelToPlugin,
          plugin_path, client));
}

void PluginService::FinishOpenChannelToPlugin(
    const FilePath& plugin_path,
    PluginProcessHost::Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  PluginProcessHost* plugin_host = FindOrStartNpapiPluginProcess(plugin_path);
  if (plugin_host)
    plugin_host->OpenChannelToPlugin(client);
  else
    client->OnError();
}

bool PluginService::GetPluginInfo(int render_process_id,
                                  int render_view_id,
                                  const GURL& url,
                                  const std::string& mime_type,
                                  webkit::npapi::WebPluginInfo* info,
                                  std::string* actual_mime_type) {
  // GetPluginInfoArray may need to load the plugins, so we need to be
  // on the FILE thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool allow_wildcard = true;
  {
    base::AutoLock auto_lock(overridden_plugins_lock_);
    for (size_t i = 0; i < overridden_plugins_.size(); ++i) {
      if (overridden_plugins_[i].render_process_id == render_process_id &&
          overridden_plugins_[i].render_view_id == render_view_id &&
          overridden_plugins_[i].url == url) {
        if (actual_mime_type)
          *actual_mime_type = mime_type;
        *info = overridden_plugins_[i].plugin;
        return true;
      }
    }
  }
  return webkit::npapi::PluginList::Singleton()->GetPluginInfo(
      url, mime_type, allow_wildcard, info, actual_mime_type);
}

void PluginService::OnWaitableEventSignaled(
    base::WaitableEvent* waitable_event) {
#if defined(OS_WIN)
  if (waitable_event == hkcu_event_.get()) {
    hkcu_key_.StartWatching();
  } else {
    hklm_key_.StartWatching();
  }

  webkit::npapi::PluginList::Singleton()->RefreshPlugins();
  PurgePluginListCache(true);
#else
  // This event should only get signaled on a Windows machine.
  NOTREACHED();
#endif  // defined(OS_WIN)
}

void PluginService::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  switch (type.value) {
#if defined(OS_MACOSX)
    case NotificationType::APP_ACTIVATED: {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              NewRunnableFunction(&NotifyPluginsOfActivation));
      break;
    }
#endif

    case NotificationType::PLUGIN_ENABLE_STATUS_CHANGED: {
      webkit::npapi::PluginList::Singleton()->RefreshPlugins();
      PurgePluginListCache(false);
      break;
    }
    case NotificationType::RENDERER_PROCESS_CLOSED: {
      int render_process_id = Source<RenderProcessHost>(source).ptr()->id();

      base::AutoLock auto_lock(overridden_plugins_lock_);
      for (size_t i = 0; i < overridden_plugins_.size(); ++i) {
        if (overridden_plugins_[i].render_process_id == render_process_id) {
          overridden_plugins_.erase(overridden_plugins_.begin() + i);
          break;
        }
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void PluginService::OverridePluginForTab(const OverriddenPlugin& plugin) {
  base::AutoLock auto_lock(overridden_plugins_lock_);
  overridden_plugins_.push_back(plugin);
}

void PluginService::PurgePluginListCache(bool reload_pages) {
  for (RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->Send(new ViewMsg_PurgePluginListCache(reload_pages));
  }
}

void PluginService::RestrictPluginToUrl(const FilePath& plugin_path,
                                        const GURL& url) {
  base::AutoLock auto_lock(restricted_plugin_lock_);
  if (url.is_empty()) {
    restricted_plugin_.erase(plugin_path);
  } else {
    restricted_plugin_[plugin_path] = url;
  }
}

bool PluginService::PluginAllowedForURL(const FilePath& plugin_path,
                                        const GURL& url) {
  if (url.is_empty())
    return true;  // Caller wants all plugins.

  base::AutoLock auto_lock(restricted_plugin_lock_);

  RestrictedPluginMap::iterator it = restricted_plugin_.find(plugin_path);
  if (it == restricted_plugin_.end())
    return true;  // This plugin is not restricted, so it's allowed everywhere.

  const GURL& required_url = it->second;
  return (url.scheme() == required_url.scheme() &&
          url.host() == required_url.host());
}

void PluginService::RegisterPepperPlugins() {
  // TODO(abarth): It seems like the PepperPluginRegistry should do this work.
  PepperPluginRegistry::ComputeList(&ppapi_plugins_);
  for (size_t i = 0; i < ppapi_plugins_.size(); ++i) {
    webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(
        ppapi_plugins_[i].ToWebPluginInfo());
  }
}

// There should generally be very few plugins so a brute-force search is fine.
PepperPluginInfo* PluginService::GetRegisteredPpapiPluginInfo(
    const FilePath& plugin_path) {
  PepperPluginInfo* info = NULL;
  for (size_t i = 0; i < ppapi_plugins_.size(); i++) {
   if (ppapi_plugins_[i].path == plugin_path) {
     info = &ppapi_plugins_[i];
     break;
   }
  }
  return info;
}

#if defined(OS_LINUX)
// static
void PluginService::RegisterFilePathWatcher(
    FilePathWatcher *watcher,
    const FilePath& path,
    FilePathWatcher::Delegate* delegate) {
  bool result = watcher->Watch(path, delegate);
  DCHECK(result);
}
#endif
