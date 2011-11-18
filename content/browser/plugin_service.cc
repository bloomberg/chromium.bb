// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/plugin_loader_posix.h"
#include "content/browser/plugin_service_filter.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/resource_context.h"
#include "content/browser/utility_process_host.h"
#include "content/common/pepper_plugin_registry.h"
#include "content/common/plugin_messages.h"
#include "content/common/utility_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
using ::base::files::FilePathWatcher;
#endif

using content::BrowserThread;
using content::PluginServiceFilter;

namespace {

// Helper function that merely runs the callback with the result. Called on the
// thread on which the original GetPlugins() call was made.
static void RunGetPluginsCallback(
    const PluginService::GetPluginsCallback& callback,
    const std::vector<webkit::WebPluginInfo>& result) {
  callback.Run(result);
}

// A callback for GetPlugins() that then gets the freshly loaded plugin groups
// and runs the callback for GetPluginGroups().
static void GetPluginsForGroupsCallback(
    const PluginService::GetPluginGroupsCallback& callback,
    const std::vector<webkit::WebPluginInfo>& plugins) {
  std::vector<webkit::npapi::PluginGroup> groups;
  webkit::npapi::PluginList::Singleton()->GetPluginGroups(false, &groups);
  callback.Run(groups);
}

// Callback set on the PluginList to assert that plugin loading happens on the
// correct thread.
void WillLoadPluginsCallback() {
#if defined(OS_WIN)
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
#else
  CHECK(false) << "Plugin loading should happen out-of-process.";
#endif
}

}  // namespace

#if defined(OS_MACOSX)
static void NotifyPluginsOfActivation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (BrowserChildProcessHost::Iterator iter(ChildProcessInfo::PLUGIN_PROCESS);
       !iter.Done(); ++iter) {
    PluginProcessHost* plugin = static_cast<PluginProcessHost*>(*iter);
    plugin->OnAppActivation();
  }
}
#elif defined(OS_POSIX) && !defined(OS_OPENBSD)
// Delegate class for monitoring directories.
class PluginDirWatcherDelegate : public FilePathWatcher::Delegate {
  virtual void OnFilePathChanged(const FilePath& path) OVERRIDE {
    VLOG(1) << "Watched path changed: " << path.value();
    // Make the plugin list update itself
    webkit::npapi::PluginList::Singleton()->RefreshPlugins();
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&PluginService::PurgePluginListCache,
                   static_cast<content::BrowserContext*>(NULL), false));
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
    : plugin_list_(NULL),
      ui_locale_(
          content::GetContentClient()->browser()->GetApplicationLocale()),
      filter_(NULL) {
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
  // Make sure no plugin channel requests have been leaked.
  DCHECK(pending_plugin_clients_.empty());
}

void PluginService::Init() {
  if (!plugin_list_)
    plugin_list_ = webkit::npapi::PluginList::Singleton();

  plugin_list()->set_will_load_plugins_callback(
      base::Bind(&WillLoadPluginsCallback));

  RegisterPepperPlugins();

  content::GetContentClient()->AddNPAPIPlugins(plugin_list());

  // Load any specified on the command line as well.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  FilePath path = command_line->GetSwitchValuePath(switches::kLoadPlugin);
  if (!path.empty())
    AddExtraPluginPath(path);
  path = command_line->GetSwitchValuePath(switches::kExtraPluginDir);
  if (!path.empty())
    plugin_list()->AddExtraPluginDir(path);

#if defined(OS_MACOSX)
  // We need to know when the browser comes forward so we can bring modal plugin
  // windows forward too.
  registrar_.Add(this, content::NOTIFICATION_APP_ACTIVATED,
                 content::NotificationService::AllSources());
#endif
}

void PluginService::StartWatchingPlugins() {
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
#elif defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
// The FilePathWatcher produces too many false positives on MacOS (access time
// updates?) which will lead to enforcing updates of the plugins way too often.
// On ChromeOS the user can't install plugins anyway and on Windows all
// important plugins register themselves in the registry so no need to do that.
  file_watcher_delegate_ = new PluginDirWatcherDelegate();
  // Get the list of all paths for registering the FilePathWatchers
  // that will track and if needed reload the list of plugins on runtime.
  std::vector<FilePath> plugin_dirs;
  plugin_list()->GetPluginDirectories(&plugin_dirs);

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

PpapiPluginProcessHost* PluginService::FindPpapiBrokerProcess(
    const FilePath& broker_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (BrowserChildProcessHost::Iterator iter(
           ChildProcessInfo::PPAPI_BROKER_PROCESS);
       !iter.Done(); ++iter) {
    PpapiPluginProcessHost* broker =
        static_cast<PpapiPluginProcessHost*>(*iter);
    if (broker->plugin_path() == broker_path)
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

  webkit::WebPluginInfo info;
  if (!GetPluginInfoByPath(plugin_path, &info)) {
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
    PpapiPluginProcessHost::PluginClient* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  PpapiPluginProcessHost* plugin_host = FindPpapiPluginProcess(plugin_path);
  if (plugin_host)
    return plugin_host;

  // Validate that the plugin is actually registered.
  content::PepperPluginInfo* info = GetRegisteredPpapiPluginInfo(plugin_path);
  if (!info)
    return NULL;

  // This plugin isn't loaded by any plugin process, so create a new process.
  return PpapiPluginProcessHost::CreatePluginHost(
      *info,
      client->GetResourceContext()->host_resolver());
}

PpapiPluginProcessHost* PluginService::FindOrStartPpapiBrokerProcess(
    const FilePath& plugin_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  PpapiPluginProcessHost* plugin_host = FindPpapiBrokerProcess(plugin_path);
  if (plugin_host)
    return plugin_host;

  // Validate that the plugin is actually registered.
  content::PepperPluginInfo* info = GetRegisteredPpapiPluginInfo(plugin_path);
  if (!info)
    return NULL;

  // TODO(ddorwin): Uncomment once out of process is supported.
  // DCHECK(info->is_out_of_process);

  // This broker isn't loaded by any broker process, so create a new process.
  return PpapiPluginProcessHost::CreateBrokerHost(*info);
}

void PluginService::OpenChannelToNpapiPlugin(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const GURL& page_url,
    const std::string& mime_type,
    PluginProcessHost::Client* client) {
  DCHECK(!ContainsKey(pending_plugin_clients_, client));
  pending_plugin_clients_.insert(client);

  // Make sure plugins are loaded if necessary.
  content::PluginServiceFilterParams params = {
    render_process_id,
    render_view_id,
    page_url,
    &client->GetResourceContext()
  };
  GetPlugins(
      base::Bind(&PluginService::ForwardGetAllowedPluginForOpenChannelToPlugin,
                 base::Unretained(this), params, url, mime_type, client));
}

void PluginService::OpenChannelToPpapiPlugin(
    const FilePath& path,
    PpapiPluginProcessHost::PluginClient* client) {
  PpapiPluginProcessHost* plugin_host = FindOrStartPpapiPluginProcess(
      path, client);
  if (plugin_host)
    plugin_host->OpenChannelToPlugin(client);
  else  // Send error.
    client->OnChannelOpened(base::kNullProcessHandle, IPC::ChannelHandle());
}

void PluginService::OpenChannelToPpapiBroker(
    const FilePath& path,
    PpapiPluginProcessHost::BrokerClient* client) {
  PpapiPluginProcessHost* plugin_host = FindOrStartPpapiBrokerProcess(path);
  if (plugin_host)
    plugin_host->OpenChannelToPlugin(client);
  else  // Send error.
    client->OnChannelOpened(base::kNullProcessHandle, IPC::ChannelHandle());
}

void PluginService::CancelOpenChannelToNpapiPlugin(
    PluginProcessHost::Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(ContainsKey(pending_plugin_clients_, client));
  pending_plugin_clients_.erase(client);
}

void PluginService::ForwardGetAllowedPluginForOpenChannelToPlugin(
    const content::PluginServiceFilterParams& params,
    const GURL& url,
    const std::string& mime_type,
    PluginProcessHost::Client* client,
    const std::vector<webkit::WebPluginInfo>&) {
  GetAllowedPluginForOpenChannelToPlugin(params.render_process_id,
      params.render_view_id, url, params.page_url, mime_type, client,
      params.resource_context);
}

void PluginService::GetAllowedPluginForOpenChannelToPlugin(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const GURL& page_url,
    const std::string& mime_type,
    PluginProcessHost::Client* client,
    const content::ResourceContext* resource_context) {
  webkit::WebPluginInfo info;
  bool allow_wildcard = true;
  bool found = GetPluginInfo(
      render_process_id, render_view_id, *resource_context,
      url, page_url, mime_type, allow_wildcard,
      NULL, &info, NULL);
  FilePath plugin_path;
  if (found)
    plugin_path = info.path;

  // Now we jump back to the IO thread to finish opening the channel.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PluginService::FinishOpenChannelToPlugin,
                 base::Unretained(this), plugin_path, client));
}

void PluginService::FinishOpenChannelToPlugin(
    const FilePath& plugin_path,
    PluginProcessHost::Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Make sure it hasn't been canceled yet.
  if (!ContainsKey(pending_plugin_clients_, client))
    return;
  pending_plugin_clients_.erase(client);

  PluginProcessHost* plugin_host = FindOrStartNpapiPluginProcess(plugin_path);
  if (plugin_host) {
    client->OnFoundPluginProcessHost(plugin_host);
    plugin_host->OpenChannelToPlugin(client);
  } else {
    client->OnError();
  }
}

bool PluginService::GetPluginInfoArray(
    const GURL& url,
    const std::string& mime_type,
    bool allow_wildcard,
    std::vector<webkit::WebPluginInfo>* plugins,
    std::vector<std::string>* actual_mime_types) {
  bool use_stale = false;
  plugin_list()->GetPluginInfoArray(url, mime_type, allow_wildcard,
                                    &use_stale, plugins, actual_mime_types);
  return use_stale;
}

bool PluginService::GetPluginInfo(int render_process_id,
                                  int render_view_id,
                                  const content::ResourceContext& context,
                                  const GURL& url,
                                  const GURL& page_url,
                                  const std::string& mime_type,
                                  bool allow_wildcard,
                                  bool* is_stale,
                                  webkit::WebPluginInfo* info,
                                  std::string* actual_mime_type) {
  std::vector<webkit::WebPluginInfo> plugins;
  std::vector<std::string> mime_types;
  bool stale = GetPluginInfoArray(
      url, mime_type, allow_wildcard, &plugins, &mime_types);
  if (is_stale)
    *is_stale = stale;
  if (plugins.size() > 1 &&
      plugins.back().path ==
          FilePath(webkit::npapi::kDefaultPluginLibraryName)) {
    // If there is at least one plug-in handling the required MIME type (apart
    // from the default plug-in), we don't need the default plug-in.
    plugins.pop_back();
  }

  for (size_t i = 0; i < plugins.size(); ++i) {
    if (!filter_ || filter_->ShouldUsePlugin(render_process_id,
                                             render_view_id,
                                             &context,
                                             url,
                                             page_url,
                                             &plugins[i])) {
      *info = plugins[i];
      if (actual_mime_type)
        *actual_mime_type = mime_types[i];
      return true;
    }
  }
  return false;
}

bool PluginService::GetPluginInfoByPath(const FilePath& plugin_path,
                                        webkit::WebPluginInfo* info) {
  std::vector<webkit::WebPluginInfo> plugins;
  plugin_list()->GetPluginsIfNoRefreshNeeded(&plugins);

  for (std::vector<webkit::WebPluginInfo>::iterator it = plugins.begin();
       it != plugins.end();
       ++it) {
    if (it->path == plugin_path) {
      *info = *it;
      return true;
    }
  }

  return false;
}

void PluginService::GetPlugins(const GetPluginsCallback& callback) {
  scoped_refptr<base::MessageLoopProxy> target_loop(
      MessageLoop::current()->message_loop_proxy());

#if defined(OS_WIN)
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&PluginService::GetPluginsInternal, base::Unretained(this),
          target_loop, callback));
#else
  std::vector<webkit::WebPluginInfo> cached_plugins;
  if (plugin_list()->GetPluginsIfNoRefreshNeeded(&cached_plugins)) {
    // Can't assume the caller is reentrant.
    target_loop->PostTask(FROM_HERE,
        base::Bind(&RunGetPluginsCallback, callback, cached_plugins));
  } else {
    // If we switch back to loading plugins in process, then we need to make
    // sure g_thread_init() gets called since plugins may call glib at load.
    if (!plugin_loader_.get())
      plugin_loader_ = new PluginLoaderPosix;
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&PluginLoaderPosix::LoadPlugins, plugin_loader_,
                   target_loop, callback));
  }
#endif
}

void PluginService::GetPluginGroups(const GetPluginGroupsCallback& callback) {
  GetPlugins(base::Bind(&GetPluginsForGroupsCallback, callback));
}

void PluginService::GetPluginsInternal(
     base::MessageLoopProxy* target_loop,
     const PluginService::GetPluginsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::vector<webkit::WebPluginInfo> plugins;
  plugin_list()->GetPlugins(&plugins);

  target_loop->PostTask(FROM_HERE,
      base::Bind(&RunGetPluginsCallback, callback, plugins));
}

void PluginService::OnWaitableEventSignaled(
    base::WaitableEvent* waitable_event) {
#if defined(OS_WIN)
  if (waitable_event == hkcu_event_.get()) {
    hkcu_key_.StartWatching();
  } else {
    hklm_key_.StartWatching();
  }

  plugin_list()->RefreshPlugins();
  PurgePluginListCache(NULL, false);
#else
  // This event should only get signaled on a Windows machine.
  NOTREACHED();
#endif  // defined(OS_WIN)
}

void PluginService::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
#if defined(OS_MACOSX)
  if (type == content::NOTIFICATION_APP_ACTIVATED) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            NewRunnableFunction(&NotifyPluginsOfActivation));
    return;
  }
#endif
  NOTREACHED();
}

void PluginService::PurgePluginListCache(
    content::BrowserContext* browser_context,
    bool reload_pages) {
  for (content::RenderProcessHost::iterator it =
          content::RenderProcessHost::AllHostsIterator();
       !it.IsAtEnd(); it.Advance()) {
    content::RenderProcessHost* host = it.GetCurrentValue();
    if (!browser_context || host->GetBrowserContext() == browser_context)
      host->Send(new ViewMsg_PurgePluginListCache(reload_pages));
  }
}

void PluginService::RegisterPepperPlugins() {
  // TODO(abarth): It seems like the PepperPluginRegistry should do this work.
  PepperPluginRegistry::ComputeList(&ppapi_plugins_);
  for (size_t i = 0; i < ppapi_plugins_.size(); ++i) {
    RegisterInternalPlugin(ppapi_plugins_[i].ToWebPluginInfo());
  }
}

// There should generally be very few plugins so a brute-force search is fine.
content::PepperPluginInfo* PluginService::GetRegisteredPpapiPluginInfo(
    const FilePath& plugin_path) {
  content::PepperPluginInfo* info = NULL;
  for (size_t i = 0; i < ppapi_plugins_.size(); i++) {
    if (ppapi_plugins_[i].path == plugin_path) {
      info = &ppapi_plugins_[i];
      break;
    }
  }
  if (info)
    return info;
  // We did not find the plugin in our list. But wait! the plugin can also
  // be a latecomer, as it happens with pepper flash. This information
  // can be obtained from the PluginList singleton and we can use it to
  // construct it and add it to the list. This same deal needs to be done
  // in the renderer side in PepperPluginRegistry.
  webkit::WebPluginInfo webplugin_info;
  if (!GetPluginInfoByPath(plugin_path, &webplugin_info))
    return NULL;
  content::PepperPluginInfo new_pepper_info;
  if (!MakePepperPluginInfo(webplugin_info, &new_pepper_info))
    return NULL;
  ppapi_plugins_.push_back(new_pepper_info);
  return &ppapi_plugins_[ppapi_plugins_.size() - 1];
}

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
// static
void PluginService::RegisterFilePathWatcher(
    FilePathWatcher *watcher,
    const FilePath& path,
    FilePathWatcher::Delegate* delegate) {
  bool result = watcher->Watch(path, delegate);
  DCHECK(result);
}
#endif

void PluginService::RefreshPlugins() {
  plugin_list()->RefreshPlugins();
}

void PluginService::AddExtraPluginPath(const FilePath& path) {
  plugin_list()->AddExtraPluginPath(path);
}

void PluginService::RemoveExtraPluginPath(const FilePath& path) {
  plugin_list()->RemoveExtraPluginPath(path);
}

void PluginService::UnregisterInternalPlugin(const FilePath& path) {
  plugin_list()->UnregisterInternalPlugin(path);
}

void PluginService::SetPluginListForTesting(
    webkit::npapi::PluginList* plugin_list) {
  plugin_list_ = plugin_list;
}

void PluginService::RegisterInternalPlugin(const webkit::WebPluginInfo& info) {
  plugin_list()->RegisterInternalPlugin(info);
}

string16 PluginService::GetPluginGroupName(const std::string& plugin_name) {
  return plugin_list()->GetPluginGroupName(plugin_name);
}
