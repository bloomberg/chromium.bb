// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class responds to requests from renderers for the list of plugins, and
// also a proxy object for plugin instances.

#ifndef CHROME_BROWSER_PLUGIN_SERVICE_H_
#define CHROME_BROWSER_PLUGIN_SERVICE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/singleton.h"
#include "base/waitable_event_watcher.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel_handle.h"

#if defined(OS_WIN)
#include "base/scoped_ptr.h"
#include "base/win/registry.h"
#endif

#if defined(OS_CHROMEOS)
namespace chromeos {
class PluginSelectionPolicy;
}
#endif

namespace IPC {
class Message;
}

class MessageLoop;
class Profile;
class ResourceDispatcherHost;
class URLRequestContext;


namespace webkit {
namespace npapi {
struct WebPluginInfo;
}
}

// This must be created on the main thread but it's only called on the IO/file
// thread.
class PluginService
    : public base::WaitableEventWatcher::Delegate,
      public NotificationObserver {
 public:
  // Initializes the global instance; should be called on startup from the main
  // thread.
  static void InitGlobalInstance(Profile* profile);

  // Returns the PluginService singleton.
  static PluginService* GetInstance();

  // Load all the plugins that should be loaded for the lifetime of the browser
  // (ie, with the LoadOnStartup flag set).
  void LoadChromePlugins(ResourceDispatcherHost* resource_dispatcher_host);

  // Sets/gets the data directory that Chrome plugins should use to store
  // persistent data.
  void SetChromePluginDataDir(const FilePath& data_dir);
  const FilePath& GetChromePluginDataDir();

  // Gets the browser's UI locale.
  const std::string& GetUILocale();

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. Returns NULL if no process has been
  // started.
  PluginProcessHost* FindPluginProcess(const FilePath& plugin_path);

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. This will start a process to host the
  // 'plugin_path' if needed. If the process fails to start, the return value
  // is NULL. Must be called on the IO thread.
  PluginProcessHost* FindOrStartPluginProcess(const FilePath& plugin_path);

  // Opens a channel to a plugin process for the given mime type, starting
  // a new plugin process if necessary.  This must be called on the IO thread
  // or else a deadlock can occur.
  void OpenChannelToPlugin(const GURL& url,
                           const std::string& mime_type,
                           PluginProcessHost::Client* client);

  // Gets the first allowed plugin in the list of plugins that matches
  // the given url and mime type.  Must be called on the FILE thread.
  bool GetFirstAllowedPluginInfo(const GURL& url,
                                 const std::string& mime_type,
                                 webkit::npapi::WebPluginInfo* info,
                                 std::string* actual_mime_type);

  // Returns true if the given plugin is allowed to be used by a page with
  // the given URL.
  bool PrivatePluginAllowedForURL(const FilePath& plugin_path, const GURL& url);

  // The UI thread's message loop
  MessageLoop* main_message_loop() { return main_message_loop_; }

  ResourceDispatcherHost* resource_dispatcher_host() const {
    return resource_dispatcher_host_;
  }

  static void EnableChromePlugins(bool enable);

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

  // Helper so we can do the plugin lookup on the FILE thread.
  void GetAllowedPluginForOpenChannelToPlugin(
      const GURL& url,
      const std::string& mime_type,
      PluginProcessHost::Client* client);

  // Helper so we can finish opening the channel after looking up the
  // plugin.
  void FinishOpenChannelToPlugin(
      const FilePath& plugin_path,
      PluginProcessHost::Client* client);

  // mapping between plugin path and PluginProcessHost
  typedef base::hash_map<FilePath, PluginProcessHost*> PluginMap;
  PluginMap plugin_hosts_;

  // The main thread's message loop.
  MessageLoop* main_message_loop_;

  // The IO thread's resource dispatcher host.
  ResourceDispatcherHost* resource_dispatcher_host_;

  // The data directory that Chrome plugins should use to store persistent data.
  FilePath chrome_plugin_data_dir_;

  // The browser's UI locale.
  const std::string ui_locale_;

  // Map of plugin paths to the origin they are restricted to.  Used for
  // extension-only plugins.
  typedef base::hash_map<FilePath, GURL> PrivatePluginMap;
  PrivatePluginMap private_plugins_;

  NotificationRegistrar registrar_;

#if defined(OS_CHROMEOS)
  scoped_refptr<chromeos::PluginSelectionPolicy> plugin_selection_policy_;
#endif

#if defined(OS_WIN)
  // Registry keys for getting notifications when new plugins are installed.
  base::win::RegKey hkcu_key_;
  base::win::RegKey hklm_key_;
  scoped_ptr<base::WaitableEvent> hkcu_event_;
  scoped_ptr<base::WaitableEvent> hklm_event_;
  base::WaitableEventWatcher hkcu_watcher_;
  base::WaitableEventWatcher hklm_watcher_;
#endif

  // Set to true if chrome plugins are enabled. Defaults to true.
  static bool enable_chrome_plugins_;

  DISALLOW_COPY_AND_ASSIGN(PluginService);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(PluginService);

#endif  // CHROME_BROWSER_PLUGIN_SERVICE_H_
