// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_loader_posix.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/utility_process_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/plugin_list.h"
#include "content/common/utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/user_metrics.h"

namespace content {

PluginLoaderPosix::PluginLoaderPosix()
    : next_load_index_(0), loading_plugins_(false) {
}

void PluginLoaderPosix::GetPlugins(
    const PluginService::GetPluginsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<WebPluginInfo> cached_plugins;
  if (PluginList::Singleton()->GetPluginsNoRefresh(&cached_plugins)) {
    // Can't assume the caller is reentrant.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, cached_plugins));
    return;
  }

  if (!loading_plugins_) {
    loading_plugins_ = true;
    callbacks_.push_back(callback);

    // When |loading_plugins_| is set to false, this instance must call
    // SetPlugins().
    PluginList::Singleton()->PrepareForPluginLoading();

    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&PluginLoaderPosix::GetPluginsToLoad,
                                       make_scoped_refptr(this)));
  } else {
    // If we are currently loading plugins, the plugin list might have been
    // invalidated in the mean time, or might get invalidated before we finish.
    // We'll wait until we have finished the current run, then try to get them
    // again from the plugin list. If it has indeed been invalidated, it will
    // restart plugin loading, otherwise it will immediately run the callback.
    callbacks_.push_back(base::Bind(&PluginLoaderPosix::GetPluginsWrapper,
                                    make_scoped_refptr(this), callback));
  }
}

bool PluginLoaderPosix::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginLoaderPosix, message)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_LoadedPlugin, OnPluginLoaded)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_LoadPluginFailed, OnPluginLoadFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PluginLoaderPosix::OnProcessCrashed(int exit_code) {
  RecordAction(
      base::UserMetricsAction("PluginLoaderPosix.UtilityProcessCrashed"));

  if (next_load_index_ == canonical_list_.size()) {
    // How this case occurs is unknown. See crbug.com/111935.
    canonical_list_.clear();
  } else {
    canonical_list_.erase(canonical_list_.begin(),
                          canonical_list_.begin() + next_load_index_ + 1);
  }

  next_load_index_ = 0;

  LoadPluginsInternal();
}

void PluginLoaderPosix::OnProcessLaunchFailed() {
  FinishedLoadingPlugins();
}

bool PluginLoaderPosix::Send(IPC::Message* message) {
  if (process_host_.get())
    return process_host_->Send(message);
  return false;
}

PluginLoaderPosix::~PluginLoaderPosix() {
}

void PluginLoaderPosix::GetPluginsToLoad() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  base::TimeTicks start_time(base::TimeTicks::Now());

  loaded_plugins_.clear();
  next_load_index_ = 0;

  canonical_list_.clear();
  PluginList::Singleton()->GetPluginPathsToLoad(
      &canonical_list_,
      PluginService::GetInstance()->NPAPIPluginsSupported());

  internal_plugins_.clear();
  PluginList::Singleton()->GetInternalPlugins(&internal_plugins_);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PluginLoaderPosix::LoadPluginsInternal,
                 make_scoped_refptr(this)));

  LOCAL_HISTOGRAM_TIMES("PluginLoaderPosix.GetPluginList",
                        (base::TimeTicks::Now() - start_time) *
                            base::Time::kMicrosecondsPerMillisecond);
}

void PluginLoaderPosix::LoadPluginsInternal() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Check if the list is empty or all plugins have already been loaded before
  // forking.
  if (IsFinishedLoadingPlugins()) {
    FinishedLoadingPlugins();
    return;
  }

  RecordAction(
      base::UserMetricsAction("PluginLoaderPosix.LaunchUtilityProcess"));

  UtilityProcessHostImpl* host = new UtilityProcessHostImpl(
      this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get());
  process_host_ = host->AsWeakPtr();
  process_host_->DisableSandbox();

  bool launched = LaunchUtilityProcess();
  if (!launched) {
    // The utility process either failed to start or failed to receive the IPC.
    // This process will never receive any callbacks for OnPluginLoaded() or
    // OnPluginLoadFailed().
    FinishedLoadingPlugins();
  }
}

void PluginLoaderPosix::GetPluginsWrapper(
    const PluginService::GetPluginsCallback& callback,
    const std::vector<WebPluginInfo>& plugins_unused) {
  // We are being called after plugin loading has finished, but we don't know
  // whether the plugin list has been invalidated in the mean time
  // (and therefore |plugins| might already be stale). So we simply ignore it
  // and call regular GetPlugins() instead.
  GetPlugins(callback);
}

void PluginLoaderPosix::OnPluginLoaded(uint32_t index,
                                       const WebPluginInfo& plugin) {
  if (index != next_load_index_) {
    LOG(ERROR) << "Received unexpected plugin load message for "
               << plugin.path.value() << "; index=" << index;
    return;
  }

  auto it = FindInternalPlugin(plugin.path);
  if (it != internal_plugins_.end()) {
    loaded_plugins_.push_back(*it);
    internal_plugins_.erase(it);
  } else {
    loaded_plugins_.push_back(plugin);
  }

  ++next_load_index_;

  if (IsFinishedLoadingPlugins())
    FinishedLoadingPlugins();
}

void PluginLoaderPosix::OnPluginLoadFailed(uint32_t index,
                                           const base::FilePath& plugin_path) {
  if (index != next_load_index_) {
    LOG(ERROR) << "Received unexpected plugin load failure message for "
               << plugin_path.value() << "; index=" << index;
    return;
  }

  ++next_load_index_;

  auto it = FindInternalPlugin(plugin_path);
  if (it != internal_plugins_.end()) {
    loaded_plugins_.push_back(*it);
    internal_plugins_.erase(it);
  }

  if (IsFinishedLoadingPlugins())
    FinishedLoadingPlugins();
}

std::vector<WebPluginInfo>::iterator PluginLoaderPosix::FindInternalPlugin(
    const base::FilePath& plugin_path) {
  return std::find_if(internal_plugins_.begin(), internal_plugins_.end(),
                      [&plugin_path](const WebPluginInfo& plugin) {
    return plugin.path == plugin_path;
  });
}

bool PluginLoaderPosix::IsFinishedLoadingPlugins() {
  if (canonical_list_.empty())
    return true;

  DCHECK(next_load_index_ <= canonical_list_.size());
  return next_load_index_ == canonical_list_.size();
}

void PluginLoaderPosix::FinishedLoadingPlugins() {
  loading_plugins_ = false;
  PluginList::Singleton()->SetPlugins(loaded_plugins_);

  for (auto& callback : callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, loaded_plugins_));
  }
  callbacks_.clear();
}

bool PluginLoaderPosix::LaunchUtilityProcess() {
  return process_host_->Send(new UtilityMsg_LoadPlugins(canonical_list_));
}

}  // namespace content
