// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_data_remover.h"

#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/version.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/common/plugin_messages.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

namespace {
const char* g_flash_mime_type = "application/x-shockwave-flash";
// TODO(bauerb): Update minimum required Flash version as soon as there is one
// implementing the API.
const char* g_min_flash_version = "100";
const int64 g_timeout_ms = 10000;
}  // namespace

PluginDataRemover::PluginDataRemover()
    : is_removing_(false),
      channel_(NULL) { }

PluginDataRemover::~PluginDataRemover() {
  DCHECK(!is_removing_);
  if (channel_)
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, channel_);
}

void PluginDataRemover::StartRemoving(base::Time begin_time, Task* done_task) {
  DCHECK(!done_task_.get());
  DCHECK(!is_removing_);
  remove_start_time_ = base::Time::Now();
  begin_time_ = begin_time;

  message_loop_ = base::MessageLoopProxy::CreateForCurrentThread();
  done_task_.reset(done_task);
  is_removing_ = true;

  AddRef();
  PluginService::GetInstance()->OpenChannelToPlugin(
      GURL(), g_flash_mime_type, this);

  BrowserThread::PostDelayedTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(this, &PluginDataRemover::OnTimeout),
      g_timeout_ms);
}

int PluginDataRemover::ID() {
  // Generate an ID for the browser process.
  return ChildProcessInfo::GenerateChildProcessUniqueId();
}

bool PluginDataRemover::OffTheRecord() {
  return false;
}

void PluginDataRemover::SetPluginInfo(
    const webkit::npapi::WebPluginInfo& info) {
}

void PluginDataRemover::OnChannelOpened(const IPC::ChannelHandle& handle) {
  ConnectToChannel(handle);
  Release();
}

void PluginDataRemover::ConnectToChannel(const IPC::ChannelHandle& handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If we timed out, don't bother connecting.
  if (!is_removing_)
    return;

  DCHECK(!channel_);
  channel_ = new IPC::Channel(handle, IPC::Channel::MODE_CLIENT, this);
  if (!channel_->Connect()) {
    LOG(DFATAL) << "Couldn't connect to plugin";
    SignalDone();
    return;
  }

  if (!channel_->Send(
          new PluginMsg_ClearSiteData(0, std::string(), begin_time_))) {
    LOG(DFATAL) << "Couldn't send ClearSiteData message";
    SignalDone();
  }
}

void PluginDataRemover::OnError() {
  NOTREACHED() << "Couldn't open plugin channel";
  SignalDone();
  Release();
}

void PluginDataRemover::OnClearSiteDataResult(bool success) {
  if (!success)
    LOG(DFATAL) << "ClearSiteData returned error";
  UMA_HISTOGRAM_TIMES("ClearPluginData.time",
                      base::Time::Now() - remove_start_time_);
  SignalDone();
}

void PluginDataRemover::OnTimeout() {
  NOTREACHED() << "Timed out";
  SignalDone();
}

bool PluginDataRemover::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PluginDataRemover, msg)
    IPC_MESSAGE_HANDLER(PluginHostMsg_ClearSiteDataResult,
                        OnClearSiteDataResult)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  return true;
}

void PluginDataRemover::OnChannelError() {
  LOG(DFATAL) << "Channel error";
  SignalDone();
}

void PluginDataRemover::SignalDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!is_removing_)
    return;
  is_removing_ = false;
  if (done_task_.get()) {
    message_loop_->PostTask(FROM_HERE, done_task_.release());
    message_loop_ = NULL;
  }
}

// static
bool PluginDataRemover::IsSupported() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool allow_wildcard = false;
  webkit::npapi::WebPluginInfo plugin;
  std::string mime_type;
  if (!webkit::npapi::PluginList::Singleton()->GetPluginInfo(GURL(),
                                                             g_flash_mime_type,
                                                             allow_wildcard,
                                                             &plugin,
                                                             &mime_type))
    return false;
  scoped_ptr<Version> version(
      webkit::npapi::PluginGroup::CreateVersionFromString(plugin.version));
  scoped_ptr<Version> min_version(
      Version::GetVersionFromString(g_min_flash_version));
  return plugin.enabled &&
         version.get() &&
         min_version->CompareTo(*version) == -1;
}
