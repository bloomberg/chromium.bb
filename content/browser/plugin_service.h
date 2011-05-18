// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class responds to requests from renderers for the list of plugins, and
// also a proxy object for plugin instances.

#ifndef CONTENT_BROWSER_PLUGIN_SERVICE_H_
#define CONTENT_BROWSER_PLUGIN_SERVICE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "build/build_config.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/ppapi_broker_process_host.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel_handle.h"
#include "webkit/plugins/npapi/webplugininfo.h"

#if defined(OS_WIN)
#include "base/memory/scoped_ptr.h"
#include "base/win/registry.h"
#endif

#if defined(OS_LINUX)
#include "base/files/file_path_watcher.h"
#endif

struct PepperPluginInfo;
class PluginDirWatcherDelegate;

// This must be created on the main thread but it's only called on the IO/file
// thread.
class PluginService
    : public base::WaitableEventWatcher::Delegate,
      public NotificationObserver {
 public:
  struct OverriddenPlugin {
    int render_process_id;
    int render_view_id;
    GURL url;
    webkit::npapi::WebPluginInfo plugin;
  };

  // Returns the PluginService singleton.
  static PluginService* GetInstance();

  // Gets the browser's UI locale.
  const std::string& GetUILocale();

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. Returns NULL if no process has been
  // started.
  PluginProcessHost* FindNpapiPluginProcess(const FilePath& plugin_path);
  PpapiPluginProcessHost* FindPpapiPluginProcess(const FilePath& plugin_path);
  PpapiBrokerProcessHost* FindPpapiBrokerProcess(const FilePath& broker_path);

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. This will start a process to host the
  // 'plugin_path' if needed. If the process fails to start, the return value
  // is NULL. Must be called on the IO thread.
  PluginProcessHost* FindOrStartNpapiPluginProcess(
      const FilePath& plugin_path);
  PpapiPluginProcessHost* FindOrStartPpapiPluginProcess(
      const FilePath& plugin_path,
      PpapiPluginProcessHost::Client* client);
  PpapiBrokerProcessHost* FindOrStartPpapiBrokerProcess(
      const FilePath& plugin_path);

  // Opens a channel to a plugin process for the given mime type, starting
  // a new plugin process if necessary.  This must be called on the IO thread
  // or else a deadlock can occur.
  void OpenChannelToNpapiPlugin(int render_process_id,
                                int render_view_id,
                                const GURL& url,
                                const std::string& mime_type,
                                PluginProcessHost::Client* client);
  void OpenChannelToPpapiPlugin(const FilePath& path,
                                PpapiPluginProcessHost::Client* client);
  void OpenChannelToPpapiBroker(const FilePath& path,
                                PpapiBrokerProcessHost::Client* client);

  // Gets the plugin in the list of plugins that matches the given url and mime
  // type.  Must be called on the FILE thread.
  bool GetPluginInfo(int render_process_id,
                     int render_view_id,
                     const GURL& url,
                     const std::string& mime_type,
                     webkit::npapi::WebPluginInfo* info,
                     std::string* actual_mime_type);

  // Safe to be called from any thread.
  void OverridePluginForTab(const OverriddenPlugin& plugin);

  // Restricts the given plugin to the the scheme and host of the given url.
  // Call with an empty url to reset this.
  // Can be called on any thread.
  void RestrictPluginToUrl(const FilePath& plugin_path, const GURL& url);

  // Returns true if the given plugin is allowed to be used by a page with
  // the given URL.
  // Can be called on any thread.
  bool PluginAllowedForURL(const FilePath& plugin_path, const GURL& url);

  // Tells all the renderer processes to throw away their cache of the plugin
  // list, and optionally also reload all the pages with plugins.
  // NOTE: can only be called on the UI thread.
  static void PurgePluginListCache(bool reload_pages);

 private:
  friend struct DefaultSingletonTraits<PluginService>;

  // Creates the PluginService object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  PluginService();
  ~PluginService();

  // base::WaitableEventWatcher::Delegate implementation.
  virtual void OnWaitableEventSignaled(base::WaitableEvent* waitable_event);

  // NotificationObserver implementation
  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details);

  void RegisterPepperPlugins();

  PepperPluginInfo* GetRegisteredPpapiPluginInfo(const FilePath& plugin_path);

  // Helper so we can do the plugin lookup on the FILE thread.
  void GetAllowedPluginForOpenChannelToPlugin(
      int render_process_id,
      int render_view_id,
      const GURL& url,
      const std::string& mime_type,
      PluginProcessHost::Client* client);

  // Helper so we can finish opening the channel after looking up the
  // plugin.
  void FinishOpenChannelToPlugin(
      const FilePath& plugin_path,
      PluginProcessHost::Client* client);

#if defined(OS_LINUX)
  // Registers a new FilePathWatcher for a given path.
  static void RegisterFilePathWatcher(
      base::files::FilePathWatcher* watcher,
      const FilePath& path,
      base::files::FilePathWatcher::Delegate* delegate);
#endif

  // The browser's UI locale.
  const std::string ui_locale_;

  // Map of plugin paths to the origin they are restricted to.
  base::Lock restricted_plugin_lock_;  // Guards access to restricted_plugin_.
  typedef base::hash_map<FilePath, GURL> RestrictedPluginMap;
  RestrictedPluginMap restricted_plugin_;

  NotificationRegistrar registrar_;

#if defined(OS_WIN)
  // Registry keys for getting notifications when new plugins are installed.
  base::win::RegKey hkcu_key_;
  base::win::RegKey hklm_key_;
  scoped_ptr<base::WaitableEvent> hkcu_event_;
  scoped_ptr<base::WaitableEvent> hklm_event_;
  base::WaitableEventWatcher hkcu_watcher_;
  base::WaitableEventWatcher hklm_watcher_;
#endif

#if defined(OS_LINUX)
  ScopedVector<base::files::FilePathWatcher> file_watchers_;
  scoped_refptr<PluginDirWatcherDelegate> file_watcher_delegate_;
#endif

  std::vector<PepperPluginInfo> ppapi_plugins_;

  std::vector<OverriddenPlugin> overridden_plugins_;
  base::Lock overridden_plugins_lock_;

  DISALLOW_COPY_AND_ASSIGN(PluginService);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(PluginService);

#endif  // CONTENT_BROWSER_PLUGIN_SERVICE_H_
