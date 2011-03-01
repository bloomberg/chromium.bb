// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_data_remover.h"

#include "base/command_line.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/version.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/plugin_messages.h"
#include "content/browser/browser_thread.h"
#include "content/browser/plugin_service.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

namespace {
const char* kFlashMimeType = "application/x-shockwave-flash";
// The minimum Flash Player version that implements NPP_ClearSiteData.
const char* kMinFlashVersion = "10.3";
const int64 kRemovalTimeoutMs = 10000;
const uint64 kClearAllData = 0;
}  // namespace

PluginDataRemover::PluginDataRemover()
    : mime_type_(kFlashMimeType),
      is_removing_(false),
      event_(new base::WaitableEvent(true, false)),
      channel_(NULL) { }

PluginDataRemover::~PluginDataRemover() {
  DCHECK(!is_removing_);
  if (channel_)
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, channel_);
}

base::WaitableEvent* PluginDataRemover::StartRemoving(
    const base::Time& begin_time) {
  DCHECK(!is_removing_);
  remove_start_time_ = base::Time::Now();
  begin_time_ = begin_time;

  is_removing_ = true;

  AddRef();
  PluginService::GetInstance()->OpenChannelToNpapiPlugin(
      0, 0, GURL(), mime_type_, this);

  BrowserThread::PostDelayedTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableMethod(this, &PluginDataRemover::OnTimeout),
      kRemovalTimeoutMs);

  return event_.get();
}

void PluginDataRemover::Wait() {
  base::Time start_time(base::Time::Now());
  bool result = true;
  if (is_removing_)
    result = event_->Wait();
  UMA_HISTOGRAM_TIMES("ClearPluginData.wait_at_shutdown",
                      base::Time::Now() - start_time);
  UMA_HISTOGRAM_TIMES("ClearPluginData.time_at_shutdown",
                      base::Time::Now() - remove_start_time_);
  DCHECK(result) << "Error waiting for plugin process";
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
    NOTREACHED() << "Couldn't connect to plugin";
    SignalDone();
    return;
  }

  if (!channel_->Send(
          new PluginMsg_ClearSiteData(std::string(),
                                      kClearAllData,
                                      begin_time_))) {
    NOTREACHED() << "Couldn't send ClearSiteData message";
    SignalDone();
    return;
  }
}

void PluginDataRemover::OnError() {
  LOG(DFATAL) << "Couldn't open plugin channel";
  SignalDone();
  Release();
}

void PluginDataRemover::OnClearSiteDataResult(bool success) {
  LOG_IF(DFATAL, !success) << "ClearSiteData returned error";
  UMA_HISTOGRAM_TIMES("ClearPluginData.time",
                      base::Time::Now() - remove_start_time_);
  SignalDone();
}

void PluginDataRemover::OnTimeout() {
  LOG_IF(DFATAL, is_removing_) << "Timed out";
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
  if (is_removing_) {
    NOTREACHED() << "Channel error";
    SignalDone();
  }
}

void PluginDataRemover::SignalDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!is_removing_)
    return;
  is_removing_ = false;
  event_->Signal();
}

// static
bool PluginDataRemover::IsSupported() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool allow_wildcard = false;
  webkit::npapi::WebPluginInfo plugin;
  std::string mime_type;
  if (!webkit::npapi::PluginList::Singleton()->GetPluginInfo(
          GURL(), kFlashMimeType, allow_wildcard, &plugin, &mime_type)) {
    return false;
  }
  scoped_ptr<Version> version(
      webkit::npapi::PluginGroup::CreateVersionFromString(plugin.version));
  scoped_ptr<Version> min_version(Version::GetVersionFromString(
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kMinClearSiteDataFlashVersion)));
  if (!min_version.get())
    min_version.reset(Version::GetVersionFromString(kMinFlashVersion));
  return webkit::npapi::IsPluginEnabled(plugin) &&
         version.get() &&
         min_version->CompareTo(*version) == -1;
}
