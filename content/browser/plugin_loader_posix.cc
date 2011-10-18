// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_loader_posix.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "content/browser/browser_thread.h"
#include "content/common/utility_messages.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

void RunGetPluginsCallback(const PluginService::GetPluginsCallback& callback,
                           const std::vector<webkit::WebPluginInfo> plugins) {
  callback.Run(plugins);
}

}  // namespace

// static
void PluginLoaderPosix::LoadPlugins(
    base::MessageLoopProxy* target_loop,
    const PluginService::GetPluginsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  PluginLoaderPosix* client = new PluginLoaderPosix(target_loop, callback);
  UtilityProcessHost* process_host =
      new UtilityProcessHost(client, BrowserThread::IO);
  process_host->set_no_sandbox(true);
#if defined(OS_MACOSX)
  process_host->set_child_flags(ChildProcessHost::CHILD_ALLOW_HEAP_EXECUTION);
#endif

  std::vector<FilePath> extra_plugin_paths;
  std::vector<FilePath> extra_plugin_dirs;
  std::vector<webkit::WebPluginInfo> internal_plugins;
  webkit::npapi::PluginList::Singleton()->GetPluginPathListsToLoad(
      &extra_plugin_paths, &extra_plugin_dirs, &internal_plugins);

  process_host->Send(new UtilityMsg_LoadPlugins(
      extra_plugin_paths, extra_plugin_dirs, internal_plugins));
}

bool PluginLoaderPosix::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginLoaderPosix, message)
    IPC_MESSAGE_HANDLER(UtilityHostMsg_LoadedPlugins, OnGotPlugins)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PluginLoaderPosix::OnProcessCrashed(int exit_code) {
  LOG(ERROR) << "Out-of-process plugin loader crashed with code " << exit_code
             << ". You will have no plugins!";
  // Don't leave callers hanging.
  OnGotPlugins(std::vector<webkit::WebPluginInfo>());
}

PluginLoaderPosix::PluginLoaderPosix(
    base::MessageLoopProxy* target_loop,
    const PluginService::GetPluginsCallback& callback)
    : target_loop_(target_loop),
      callback_(callback) {
}

PluginLoaderPosix::~PluginLoaderPosix() {
}

void PluginLoaderPosix::OnGotPlugins(
    const std::vector<webkit::WebPluginInfo>& plugins) {
  webkit::npapi::PluginList::Singleton()->SetPlugins(plugins);
  target_loop_->PostTask(FROM_HERE,
      base::Bind(&RunGetPluginsCallback, callback_, plugins));
}
