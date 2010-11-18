// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_data_remover.h"

#include "base/message_loop_proxy.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/common/plugin_messages.h"
#include "ipc/ipc_channel.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

namespace {
const std::string flash_mime_type = "application/x-shockwave-flash";
const int64 timeout_ms = 10000;
}

PluginDataRemover::PluginDataRemover()
    : channel_(NULL),
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PluginDataRemover::~PluginDataRemover() {
  DCHECK(!done_task_.get());
  if (!BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, channel_))
    delete channel_;
}

void PluginDataRemover::StartRemoving(base::Time begin_time, Task* done_task) {
  DCHECK(!done_task_.get());
  begin_time_ = begin_time;

  message_loop_ = base::MessageLoopProxy::CreateForCurrentThread();
  done_task_.reset(done_task);

  PluginService::GetInstance()->OpenChannelToPlugin(
      GURL(), flash_mime_type, this);

  BrowserThread::PostDelayedTask(
      BrowserThread::IO,
      FROM_HERE,
      method_factory_.NewRunnableMethod(&PluginDataRemover::OnTimeout),
      timeout_ms);
}

int PluginDataRemover::ID() {
  // Generate an ID for the browser process.
  return ChildProcessInfo::GenerateChildProcessUniqueId();
}

bool PluginDataRemover::OffTheRecord() {
  return false;
}

void PluginDataRemover::SetPluginInfo(const WebPluginInfo& info) {
}

void PluginDataRemover::OnChannelOpened(const IPC::ChannelHandle& handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!channel_);
#if defined(OS_POSIX)
  // If we received a ChannelHandle, register it now.
  if (handle.socket.fd >= 0)
    IPC::AddChannelSocket(handle.name, handle.socket.fd);
#endif
  channel_ = new IPC::Channel(handle.name, IPC::Channel::MODE_CLIENT, this);
  if (!channel_->Connect()) {
    NOTREACHED() << "Couldn't connect to plugin";
    SignalDone();
    return;
  }

  if (!channel_->Send(
          new PluginMsg_ClearSiteData(0, std::string(), begin_time_))) {
    NOTREACHED() << "Couldn't send ClearSiteData message";
    SignalDone();
  }
}

void PluginDataRemover::OnError() {
  NOTREACHED() << "Couldn't open plugin channel";
  SignalDone();
}

void PluginDataRemover::OnClearSiteDataResult(bool success) {
  DCHECK(success) << "ClearSiteData returned error";
  SignalDone();
}

void PluginDataRemover::OnTimeout() {
  NOTREACHED() << "Timed out";
  SignalDone();
}

void PluginDataRemover::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PluginDataRemover, msg)
    IPC_MESSAGE_HANDLER(PluginHostMsg_ClearSiteDataResult,
                        OnClearSiteDataResult)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void PluginDataRemover::OnChannelError() {
  NOTREACHED() << "Channel error";
  SignalDone();
}

void PluginDataRemover::SignalDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!done_task_.get())
    return;
  message_loop_->PostTask(FROM_HERE, done_task_.release());
  message_loop_ = NULL;
}
