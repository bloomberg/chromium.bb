// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PLUGIN_LOADER_POSIX_H_
#define CONTENT_BROWSER_PLUGIN_LOADER_POSIX_H_

#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/utility_process_host_client.h"
#include "ipc/ipc_sender.h"
#include "webkit/plugins/webplugininfo.h"

namespace base {
class MessageLoopProxy;
}

namespace content {
class UtilityProcessHost;

// This class is responsible for managing the out-of-process plugin loading on
// POSIX systems. It primarily lives on the IO thread, but has a brief stay on
// the FILE thread to iterate over plugin directories when it is first
// constructed.
//
// The following is the algorithm used to load plugins:
// 1. This asks the PluginList for the list of all potential plugins to attempt
//    to load. This is referred to as the canonical list.
// 2. The child process this hosts is forked and the canonical list is sent to
//    it.
// 3. The child process iterates over the canonical list, attempting to load
//    each plugin in the order specified by the list. It sends an IPC message
//    to the browser after each load, indicating success or failure. The two
//    processes synchronize the position in the vector that will be used to
//    attempt to load the next plugin.
// 4. If the child dies during this process, the host forks another child and
//    resumes loading at the position past the plugin that it just attempted to
//    load, bypassing the problematic plugin.
// 5. This algorithm continues until the canonical list has been walked to the
//    end, after which the list of loaded plugins is set on the PluginList and
//    the completion callback is run.
class CONTENT_EXPORT PluginLoaderPosix
    : public NON_EXPORTED_BASE(UtilityProcessHostClient),
      public IPC::Sender {
 public:
  PluginLoaderPosix();

  // Must be called from the IO thread.
  void LoadPlugins(
      scoped_refptr<base::MessageLoopProxy> target_loop,
      const PluginService::GetPluginsCallback& callback);

  // UtilityProcessHostClient:
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Sender:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  struct PendingCallback {
    PendingCallback(scoped_refptr<base::MessageLoopProxy> target_loop,
                    const PluginService::GetPluginsCallback& callback);
    ~PendingCallback();

    scoped_refptr<base::MessageLoopProxy> target_loop;
    PluginService::GetPluginsCallback callback;
  };

  virtual ~PluginLoaderPosix();

  // Called on the FILE thread to get the list of plugin paths to probe.
  void GetPluginsToLoad();

  // Must be called on the IO thread.
  virtual void LoadPluginsInternal();

  // Message handlers.
  void OnPluginLoaded(uint32 index, const webkit::WebPluginInfo& plugin);
  void OnPluginLoadFailed(uint32 index, const base::FilePath& plugin_path);

  // Checks if the plugin path is an internal plugin, and, if it is, adds it to
  // |loaded_plugins_|.
  bool MaybeAddInternalPlugin(const base::FilePath& plugin_path);

  // Runs all the registered callbacks on each's target loop if the condition
  // for ending the load process is done (i.e. the |next_load_index_| is outside
  // the range of the |canonical_list_|).
  bool MaybeRunPendingCallbacks();

  // The process host for which this is a client.
  base::WeakPtr<UtilityProcessHost> process_host_;

  // A list of paths to plugins which will be loaded by the utility process, in
  // the order specified by this vector.
  std::vector<base::FilePath> canonical_list_;

  // The index in |canonical_list_| of the plugin that the child process will
  // attempt to load next.
  size_t next_load_index_;

  // Internal plugins that have been registered at the time of loading.
  std::vector<webkit::WebPluginInfo> internal_plugins_;

  // A vector of plugins that have been loaded successfully.
  std::vector<webkit::WebPluginInfo> loaded_plugins_;

  // The callback and message loop on which the callback will be run when the
  // plugin loading process has been completed.
  std::deque<PendingCallback> callbacks_;

  // The time at which plugin loading started.
  base::TimeTicks load_start_time_;

  friend class MockPluginLoaderPosix;
  DISALLOW_COPY_AND_ASSIGN(PluginLoaderPosix);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_LOADER_POSIX_H_
