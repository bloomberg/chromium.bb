// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_loader_posix.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/utility_messages.h"
#include "content/browser/utility_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace content {

PluginLoaderPosix::PluginLoaderPosix()
    : next_load_index_(0) {
}

void PluginLoaderPosix::LoadPlugins(
    scoped_refptr<base::MessageLoopProxy> target_loop,
    const PluginService::GetPluginsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  callbacks_.push_back(PendingCallback(target_loop, callback));

  if (callbacks_.size() == 1) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&PluginLoaderPosix::GetPluginsToLoad, this));
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

bool PluginLoaderPosix::Send(IPC::Message* message) {
  if (process_host_)
    return process_host_->Send(message);
  return false;
}

PluginLoaderPosix::~PluginLoaderPosix() {
}

void PluginLoaderPosix::GetPluginsToLoad() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::TimeTicks start_time(base::TimeTicks::Now());

  loaded_plugins_.clear();
  next_load_index_ = 0;

  canonical_list_.clear();
  PluginServiceImpl::GetInstance()->GetPluginList()->GetPluginPathsToLoad(
      &canonical_list_);

  internal_plugins_.clear();
  PluginServiceImpl::GetInstance()->GetPluginList()->GetInternalPlugins(
      &internal_plugins_);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PluginLoaderPosix::LoadPluginsInternal,
                 make_scoped_refptr(this)));

  HISTOGRAM_TIMES("PluginLoaderPosix.GetPluginList",
                  (base::TimeTicks::Now() - start_time) *
                      base::Time::kMicrosecondsPerMillisecond);
}

void PluginLoaderPosix::LoadPluginsInternal() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Check if the list is empty or all plugins have already been loaded before
  // forking.
  if (MaybeRunPendingCallbacks())
    return;

  if (load_start_time_.is_null())
    load_start_time_ = base::TimeTicks::Now();

  UtilityProcessHostImpl* host = new UtilityProcessHostImpl(
      this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  process_host_ = host->AsWeakPtr();
  process_host_->DisableSandbox();
#if defined(OS_MACOSX)
  host->set_child_flags(ChildProcessHost::CHILD_ALLOW_HEAP_EXECUTION);
#endif

  process_host_->Send(new UtilityMsg_LoadPlugins(canonical_list_));
}

void PluginLoaderPosix::OnPluginLoaded(uint32 index,
                                       const webkit::WebPluginInfo& plugin) {
  if (index != next_load_index_) {
    LOG(ERROR) << "Received unexpected plugin load message for "
               << plugin.path.value() << "; index=" << index;
    return;
  }

  if (!MaybeAddInternalPlugin(plugin.path))
    loaded_plugins_.push_back(plugin);

  ++next_load_index_;

  MaybeRunPendingCallbacks();
}

void PluginLoaderPosix::OnPluginLoadFailed(uint32 index,
                                           const base::FilePath& plugin_path) {
  if (index != next_load_index_) {
    LOG(ERROR) << "Received unexpected plugin load failure message for "
               << plugin_path.value() << "; index=" << index;
    return;
  }

  ++next_load_index_;

  MaybeAddInternalPlugin(plugin_path);
  MaybeRunPendingCallbacks();
}

bool PluginLoaderPosix::MaybeAddInternalPlugin(
    const base::FilePath& plugin_path) {
  for (std::vector<webkit::WebPluginInfo>::iterator it =
           internal_plugins_.begin();
       it != internal_plugins_.end();
       ++it) {
    if (it->path == plugin_path) {
      loaded_plugins_.push_back(*it);
      internal_plugins_.erase(it);
      return true;
    }
  }
  return false;
}

bool PluginLoaderPosix::MaybeRunPendingCallbacks() {
  if (next_load_index_ < canonical_list_.size())
    return false;

  PluginServiceImpl::GetInstance()->GetPluginList()->SetPlugins(
      loaded_plugins_);

  // Only call the first callback with loaded plugins because there may be
  // some extra plugin paths added since the first callback is added.
  if (!callbacks_.empty()) {
    PendingCallback callback = callbacks_.front();
    callbacks_.pop_front();
    callback.target_loop->PostTask(
        FROM_HERE,
        base::Bind(callback.callback, loaded_plugins_));
  }

  HISTOGRAM_TIMES("PluginLoaderPosix.LoadDone",
                  (base::TimeTicks::Now() - load_start_time_)
                      * base::Time::kMicrosecondsPerMillisecond);
  load_start_time_ = base::TimeTicks();

  if (!callbacks_.empty()) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&PluginLoaderPosix::GetPluginsToLoad, this));
    return false;
  }
  return true;
}

PluginLoaderPosix::PendingCallback::PendingCallback(
    scoped_refptr<base::MessageLoopProxy> loop,
    const PluginService::GetPluginsCallback& cb)
    : target_loop(loop),
      callback(cb) {
}

PluginLoaderPosix::PendingCallback::~PendingCallback() {
}

}  // namespace content
