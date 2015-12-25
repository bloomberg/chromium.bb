// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class responds to requests from renderers for the list of plugins, and
// also a proxy object for plugin instances.

#ifndef CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_

#if !defined(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

#include <map>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ipc/ipc_channel_handle.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/memory/scoped_ptr.h"
#include "base/win/registry.h"
#endif

#if defined(OS_POSIX) && !defined(OS_OPENBSD) && !defined(OS_ANDROID)
#include "base/files/file_path_watcher.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
class BrowserContext;
class PluginDirWatcherDelegate;
class PluginLoaderPosix;
class PluginServiceFilter;
class ResourceContext;
struct PepperPluginInfo;

// base::Bind() has limited arity, and the filter-related methods tend to
// surpass that limit.
struct PluginServiceFilterParams {
  int render_process_id;
  int render_frame_id;
  GURL page_url;
  ResourceContext* resource_context;
};

class CONTENT_EXPORT PluginServiceImpl
    : NON_EXPORTED_BASE(public PluginService) {
 public:
  // Returns the PluginServiceImpl singleton.
  static PluginServiceImpl* GetInstance();

  // PluginService implementation:
  void Init() override;
  void StartWatchingPlugins() override;
  bool GetPluginInfoArray(const GURL& url,
                          const std::string& mime_type,
                          bool allow_wildcard,
                          std::vector<WebPluginInfo>* info,
                          std::vector<std::string>* actual_mime_types) override;
  bool GetPluginInfo(int render_process_id,
                     int render_frame_id,
                     ResourceContext* context,
                     const GURL& url,
                     const GURL& page_url,
                     const std::string& mime_type,
                     bool allow_wildcard,
                     bool* is_stale,
                     WebPluginInfo* info,
                     std::string* actual_mime_type) override;
  bool GetPluginInfoByPath(const base::FilePath& plugin_path,
                           WebPluginInfo* info) override;
  base::string16 GetPluginDisplayNameByPath(
      const base::FilePath& path) override;
  void GetPlugins(const GetPluginsCallback& callback) override;
  PepperPluginInfo* GetRegisteredPpapiPluginInfo(
      const base::FilePath& plugin_path) override;
  void SetFilter(PluginServiceFilter* filter) override;
  PluginServiceFilter* GetFilter() override;
  void ForcePluginShutdown(const base::FilePath& plugin_path) override;
  bool IsPluginUnstable(const base::FilePath& plugin_path) override;
  void RefreshPlugins() override;
  void AddExtraPluginPath(const base::FilePath& path) override;
  void RemoveExtraPluginPath(const base::FilePath& path) override;
  void AddExtraPluginDir(const base::FilePath& path) override;
  void RegisterInternalPlugin(const WebPluginInfo& info,
                              bool add_at_beginning) override;
  void UnregisterInternalPlugin(const base::FilePath& path) override;
  void GetInternalPlugins(std::vector<WebPluginInfo>* plugins) override;
  bool NPAPIPluginsSupported() override;
  void DisablePluginsDiscoveryForTesting() override;
#if defined(OS_MACOSX)
  void AppActivated() override;
#elif defined(OS_WIN)
  bool GetPluginInfoFromWindow(HWND window,
                               base::string16* plugin_name,
                               base::string16* plugin_version) override;

  // Returns true iff the given HWND is a plugin.
  bool IsPluginWindow(HWND window);
#endif
  bool PpapiDevChannelSupported(BrowserContext* browser_context,
                                const GURL& document_url) override;

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. This will start a process to host the
  // 'plugin_path' if needed. If the process fails to start, the return value
  // is NULL. Must be called on the IO thread.
  PluginProcessHost* FindOrStartNpapiPluginProcess(
      int render_process_id, const base::FilePath& plugin_path);
  PpapiPluginProcessHost* FindOrStartPpapiPluginProcess(
      int render_process_id,
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory);
  PpapiPluginProcessHost* FindOrStartPpapiBrokerProcess(
      int render_process_id, const base::FilePath& plugin_path);

  // Opens a channel to a plugin process for the given mime type, starting
  // a new plugin process if necessary.  This must be called on the IO thread
  // or else a deadlock can occur.
  void OpenChannelToNpapiPlugin(int render_process_id,
                                int render_frame_id,
                                const GURL& url,
                                const GURL& page_url,
                                const std::string& mime_type,
                                PluginProcessHost::Client* client);
  void OpenChannelToPpapiPlugin(int render_process_id,
                                const base::FilePath& plugin_path,
                                const base::FilePath& profile_data_directory,
                                PpapiPluginProcessHost::PluginClient* client);
  void OpenChannelToPpapiBroker(int render_process_id,
                                const base::FilePath& path,
                                PpapiPluginProcessHost::BrokerClient* client);

  // Cancels opening a channel to a NPAPI plugin.
  void CancelOpenChannelToNpapiPlugin(PluginProcessHost::Client* client);

  // Used to monitor plugin stability.
  void RegisterPluginCrash(const base::FilePath& plugin_path);

 private:
  friend struct base::DefaultSingletonTraits<PluginServiceImpl>;

  // Creates the PluginServiceImpl object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  PluginServiceImpl();
  ~PluginServiceImpl() override;

#if defined(OS_WIN)
  void OnKeyChanged(base::win::RegKey* key);
#endif

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. Returns NULL if no process has been
  // started.
  PluginProcessHost* FindNpapiPluginProcess(const base::FilePath& plugin_path);
  PpapiPluginProcessHost* FindPpapiPluginProcess(
      const base::FilePath& plugin_path,
      const base::FilePath& profile_data_directory);
  PpapiPluginProcessHost* FindPpapiBrokerProcess(
      const base::FilePath& broker_path);

  void RegisterPepperPlugins();

  // Run on the blocking pool to load the plugins synchronously.
  void GetPluginsInternal(base::SingleThreadTaskRunner* target_task_runner,
                          const GetPluginsCallback& callback);

#if defined(OS_POSIX)
  void GetPluginsOnIOThread(base::SingleThreadTaskRunner* target_task_runner,
                            const GetPluginsCallback& callback);
#endif

  // Binding directly to GetAllowedPluginForOpenChannelToPlugin() isn't possible
  // because more arity is needed <http://crbug.com/98542>. This just forwards.
  void ForwardGetAllowedPluginForOpenChannelToPlugin(
      const PluginServiceFilterParams& params,
      const GURL& url,
      const std::string& mime_type,
      PluginProcessHost::Client* client,
      const std::vector<WebPluginInfo>&);
  // Helper so we can do the plugin lookup on the FILE thread.
  void GetAllowedPluginForOpenChannelToPlugin(
      int render_process_id,
      int render_frame_id,
      const GURL& url,
      const GURL& page_url,
      const std::string& mime_type,
      PluginProcessHost::Client* client,
      ResourceContext* resource_context);

  // Helper so we can finish opening the channel after looking up the
  // plugin.
  void FinishOpenChannelToPlugin(int render_process_id,
                                 const base::FilePath& plugin_path,
                                 PluginProcessHost::Client* client);

#if defined(OS_POSIX) && !defined(OS_OPENBSD) && !defined(OS_ANDROID)
  // Registers a new FilePathWatcher for a given path.
  static void RegisterFilePathWatcher(base::FilePathWatcher* watcher,
                                      const base::FilePath& path);
#endif

#if defined(OS_WIN)
  // Registry keys for getting notifications when new plugins are installed.
  base::win::RegKey hkcu_key_;
  base::win::RegKey hklm_key_;
#endif

  bool npapi_plugins_enabled_;

#if defined(OS_POSIX) && !defined(OS_OPENBSD) && !defined(OS_ANDROID)
  ScopedVector<base::FilePathWatcher> file_watchers_;
#endif

  std::vector<PepperPluginInfo> ppapi_plugins_;

  // Weak pointer; outlives us.
  PluginServiceFilter* filter_;

  std::set<PluginProcessHost::Client*> pending_plugin_clients_;

  // Used to sequentialize loading plugins from disk.
  base::SequencedWorkerPool::SequenceToken plugin_list_token_;

#if defined(OS_POSIX)
  scoped_refptr<PluginLoaderPosix> plugin_loader_;
#endif

  // Used to detect if a given plugin is crashing over and over.
  std::map<base::FilePath, std::vector<base::Time> > crash_times_;

  DISALLOW_COPY_AND_ASSIGN(PluginServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_SERVICE_IMPL_H_
