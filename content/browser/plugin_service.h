// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class responds to requests from renderers for the list of plugins, and
// also a proxy object for plugin instances.

#ifndef CONTENT_BROWSER_PLUGIN_SERVICE_H_
#define CONTENT_BROWSER_PLUGIN_SERVICE_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "build/build_config.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel_handle.h"

#if defined(OS_WIN)
#include "base/memory/scoped_ptr.h"
#include "base/win/registry.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
#include "base/files/file_path_watcher.h"
#endif

class PluginDirWatcherDelegate;
class PluginLoaderPosix;

namespace base {
class MessageLoopProxy;
}

namespace content {
class BrowserContext;
class ResourceContext;
struct PepperPluginInfo;
class PluginServiceFilter;
struct PluginServiceFilterParams;
}

namespace webkit {
namespace npapi {
class PluginGroup;
class PluginList;
}
}

// This must be created on the main thread but it's only called on the IO/file
// thread. This is an asynchronous wrapper around the PluginList interface for
// querying plugin information. This must be used instead of that to avoid
// doing expensive disk operations on the IO/UI threads.
class CONTENT_EXPORT PluginService
    : public base::WaitableEventWatcher::Delegate,
      public content::NotificationObserver {
 public:
  struct OverriddenPlugin {
    int render_process_id;
    int render_view_id;
    GURL url;  // If empty, the override applies to all urls in render_view.
    webkit::WebPluginInfo plugin;
  };

  typedef base::Callback<void(const std::vector<webkit::WebPluginInfo>&)>
      GetPluginsCallback;
  typedef base::Callback<void(const std::vector<webkit::npapi::PluginGroup>&)>
      GetPluginGroupsCallback;

  // Returns the PluginService singleton.
  static PluginService* GetInstance();

  // Must be called on the instance to finish initialization.
  void Init();

  // Starts watching for changes in the list of installed plug-ins.
  void StartWatchingPlugins();

  // Gets the browser's UI locale.
  const std::string& GetUILocale();

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. Returns NULL if no process has been
  // started.
  PluginProcessHost* FindNpapiPluginProcess(const FilePath& plugin_path);
  PpapiPluginProcessHost* FindPpapiPluginProcess(const FilePath& plugin_path);
  PpapiPluginProcessHost* FindPpapiBrokerProcess(const FilePath& broker_path);

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. This will start a process to host the
  // 'plugin_path' if needed. If the process fails to start, the return value
  // is NULL. Must be called on the IO thread.
  PluginProcessHost* FindOrStartNpapiPluginProcess(
      const FilePath& plugin_path);
  PpapiPluginProcessHost* FindOrStartPpapiPluginProcess(
      const FilePath& plugin_path,
      PpapiPluginProcessHost::PluginClient* client);
  PpapiPluginProcessHost* FindOrStartPpapiBrokerProcess(
      const FilePath& plugin_path);

  // Opens a channel to a plugin process for the given mime type, starting
  // a new plugin process if necessary.  This must be called on the IO thread
  // or else a deadlock can occur.
  void OpenChannelToNpapiPlugin(int render_process_id,
                                int render_view_id,
                                const GURL& url,
                                const GURL& page_url,
                                const std::string& mime_type,
                                PluginProcessHost::Client* client);
  void OpenChannelToPpapiPlugin(const FilePath& path,
                                PpapiPluginProcessHost::PluginClient* client);
  void OpenChannelToPpapiBroker(const FilePath& path,
                                PpapiPluginProcessHost::BrokerClient* client);

  // Cancels opening a channel to a NPAPI plugin.
  void CancelOpenChannelToNpapiPlugin(PluginProcessHost::Client* client);

  // Gets the plugin in the list of plugins that matches the given url and mime
  // type. Returns true if the data is frome a stale plugin list, false if it
  // is up to date. This can be called from any thread.
  bool GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          bool allow_wildcard,
                          std::vector<webkit::WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types);

  // Gets plugin info for an individual plugin and filters the plugins using
  // the |context| and renderer IDs. This will report whether the data is stale
  // via |is_stale| and returns whether or not the plugin can be found.
  bool GetPluginInfo(int render_process_id,
                     int render_view_id,
                     const content::ResourceContext& context,
                     const GURL& url,
                     const GURL& page_url,
                     const std::string& mime_type,
                     bool allow_wildcard,
                     bool* is_stale,
                     webkit::WebPluginInfo* info,
                     std::string* actual_mime_type);

  // Get plugin info by plugin path (including disabled plugins). Returns true
  // if the plugin is found and WebPluginInfo has been filled in |info|. This
  // will use cached data in the plugin list.
  bool GetPluginInfoByPath(const FilePath& plugin_path,
                           webkit::WebPluginInfo* info);

  // Asynchronously loads plugins if necessary and then calls back to the
  // provided function on the calling MessageLoop on completion.
  void GetPlugins(const GetPluginsCallback& callback);

  // Asynchronously loads the list of plugin groups if necessary and then calls
  // back to the provided function on the calling MessageLoop on completion.
  void GetPluginGroups(const GetPluginGroupsCallback& callback);

  // Tells all the renderer processes associated with the given browser context
  // to throw away their cache of the plugin list, and optionally also reload
  // all the pages with plugins. If |browser_context| is NULL, purges the cache
  // in all renderers.
  // NOTE: can only be called on the UI thread.
  static void PurgePluginListCache(content::BrowserContext* browser_context,
                                   bool reload_pages);

  void set_filter(content::PluginServiceFilter* filter) {
    filter_ = filter;
  }
  content::PluginServiceFilter* filter() { return filter_; }

  // The following functions are wrappers around webkit::npapi::PluginList.
  // These must be used instead of those in order to ensure that we have a
  // single global list in the component build and so that we don't
  // accidentally load plugins in the wrong process or thread. Refer to
  // PluginList for further documentation of these functions.
  void RefreshPlugins();
  void AddExtraPluginPath(const FilePath& path);
  void RemoveExtraPluginPath(const FilePath& path);
  void UnregisterInternalPlugin(const FilePath& path);
  void RegisterInternalPlugin(const webkit::WebPluginInfo& info);
  string16 GetPluginGroupName(const std::string& plugin_name);

  // TODO(dpranke): This should be private.
  webkit::npapi::PluginList* plugin_list() { return plugin_list_; }

  void SetPluginListForTesting(webkit::npapi::PluginList* plugin_list);

 private:
  friend struct DefaultSingletonTraits<PluginService>;

  // Creates the PluginService object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  PluginService();
  virtual ~PluginService();

  // base::WaitableEventWatcher::Delegate implementation.
  virtual void OnWaitableEventSignaled(
      base::WaitableEvent* waitable_event) OVERRIDE;

  // content::NotificationObserver implementation
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void RegisterPepperPlugins();

  content::PepperPluginInfo* GetRegisteredPpapiPluginInfo(
      const FilePath& plugin_path);

  // Function that is run on the FILE thread to load the plugins synchronously.
  void GetPluginsInternal(base::MessageLoopProxy* target_loop,
                          const GetPluginsCallback& callback);

  // Binding directly to GetAllowedPluginForOpenChannelToPlugin() isn't possible
  // because more arity is needed <http://crbug.com/98542>. This just forwards.
  void ForwardGetAllowedPluginForOpenChannelToPlugin(
      const content::PluginServiceFilterParams& params,
      const GURL& url,
      const std::string& mime_type,
      PluginProcessHost::Client* client,
      const std::vector<webkit::WebPluginInfo>&);
  // Helper so we can do the plugin lookup on the FILE thread.
  void GetAllowedPluginForOpenChannelToPlugin(
      int render_process_id,
      int render_view_id,
      const GURL& url,
      const GURL& page_url,
      const std::string& mime_type,
      PluginProcessHost::Client* client,
      const content::ResourceContext* resource_context);

  // Helper so we can finish opening the channel after looking up the
  // plugin.
  void FinishOpenChannelToPlugin(
      const FilePath& plugin_path,
      PluginProcessHost::Client* client);

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
  // Registers a new FilePathWatcher for a given path.
  static void RegisterFilePathWatcher(
      base::files::FilePathWatcher* watcher,
      const FilePath& path,
      base::files::FilePathWatcher::Delegate* delegate);
#endif

  // The plugin list instance.
  webkit::npapi::PluginList* plugin_list_;

  // The browser's UI locale.
  const std::string ui_locale_;

  content::NotificationRegistrar registrar_;

#if defined(OS_WIN)
  // Registry keys for getting notifications when new plugins are installed.
  base::win::RegKey hkcu_key_;
  base::win::RegKey hklm_key_;
  scoped_ptr<base::WaitableEvent> hkcu_event_;
  scoped_ptr<base::WaitableEvent> hklm_event_;
  base::WaitableEventWatcher hkcu_watcher_;
  base::WaitableEventWatcher hklm_watcher_;
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
  ScopedVector<base::files::FilePathWatcher> file_watchers_;
  scoped_refptr<PluginDirWatcherDelegate> file_watcher_delegate_;
#endif

  std::vector<content::PepperPluginInfo> ppapi_plugins_;

  // Weak pointer; outlives us.
  content::PluginServiceFilter* filter_;

  std::set<PluginProcessHost::Client*> pending_plugin_clients_;

#if defined(OS_POSIX)
  scoped_refptr<PluginLoaderPosix> plugin_loader_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PluginService);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(PluginService);

#endif  // CONTENT_BROWSER_PLUGIN_SERVICE_H_
