// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_data_remover_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/waitable_event.h"
#include "base/version.h"
#include "content/browser/plugin_service.h"
#include "content/common/plugin_messages.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/plugins/npapi/plugin_group.h"

using content::BrowserThread;

namespace {

const char kFlashMimeType[] = "application/x-shockwave-flash";
// The minimum Flash Player version that implements NPP_ClearSiteData.
const char kMinFlashVersion[] = "10.3";
const int64 kRemovalTimeoutMs = 10000;
const uint64 kClearAllData = 0;

}  // namespace

namespace content {

// static
PluginDataRemover* PluginDataRemover::Create(
    const content::ResourceContext& resource_context) {
  return new PluginDataRemoverImpl(resource_context);
}

// static
bool PluginDataRemover::IsSupported(webkit::WebPluginInfo* plugin) {
  bool allow_wildcard = false;
  std::vector<webkit::WebPluginInfo> plugins;
  PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), kFlashMimeType, allow_wildcard, &plugins, NULL);
  std::vector<webkit::WebPluginInfo>::iterator plugin_it = plugins.begin();
  if (plugin_it == plugins.end())
    return false;
  scoped_ptr<Version> version(
      webkit::npapi::PluginGroup::CreateVersionFromString(plugin_it->version));
  scoped_ptr<Version> min_version(
      Version::GetVersionFromString(kMinFlashVersion));
  bool rv = version.get() && min_version->CompareTo(*version) == -1;
  if (rv)
    *plugin = *plugin_it;
  return rv;
}

}

PluginDataRemoverImpl::PluginDataRemoverImpl(
    const content::ResourceContext& resource_context)
    : mime_type_(kFlashMimeType),
      is_starting_process_(false),
      is_removing_(false),
      context_(resource_context),
      event_(new base::WaitableEvent(true, false)),
      channel_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

PluginDataRemoverImpl::~PluginDataRemoverImpl() {
  if (is_starting_process_)
    PluginService::GetInstance()->CancelOpenChannelToNpapiPlugin(this);
  DCHECK(!is_removing_);
  if (channel_)
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, channel_);
}

base::WaitableEvent* PluginDataRemoverImpl::StartRemoving(
    base::Time begin_time) {
  DCHECK(!is_removing_);
  remove_start_time_ = base::Time::Now();
  begin_time_ = begin_time;

  is_starting_process_ = true;
  is_removing_ = true;
  PluginService::GetInstance()->OpenChannelToNpapiPlugin(
      0, 0, GURL(), GURL(), mime_type_, this);

  BrowserThread::PostDelayedTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PluginDataRemoverImpl::OnTimeout, weak_factory_.GetWeakPtr()),
      kRemovalTimeoutMs);

  return event_.get();
}

int PluginDataRemoverImpl::ID() {
  // Generate a unique identifier for this PluginProcessHostClient.
  return ChildProcessInfo::GenerateChildProcessUniqueId();
}

bool PluginDataRemoverImpl::OffTheRecord() {
  return false;
}

const content::ResourceContext& PluginDataRemoverImpl::GetResourceContext() {
  return context_;
}

void PluginDataRemoverImpl::SetPluginInfo(
    const webkit::WebPluginInfo& info) {
}

void PluginDataRemoverImpl::OnFoundPluginProcessHost(
    PluginProcessHost* host) {
}

void PluginDataRemoverImpl::OnSentPluginChannelRequest() {
}

void PluginDataRemoverImpl::OnChannelOpened(const IPC::ChannelHandle& handle) {
  is_starting_process_ = false;
  ConnectToChannel(handle);
}

void PluginDataRemoverImpl::ConnectToChannel(const IPC::ChannelHandle& handle) {
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

  if (!channel_->Send(new PluginMsg_ClearSiteData(std::string(),
                                                  kClearAllData,
                                                  begin_time_))) {
    NOTREACHED() << "Couldn't send ClearSiteData message";
    SignalDone();
    return;
  }
}

void PluginDataRemoverImpl::OnError() {
  LOG(DFATAL) << "Couldn't open plugin channel";
  SignalDone();
}

void PluginDataRemoverImpl::OnClearSiteDataResult(bool success) {
  LOG_IF(ERROR, !success) << "ClearSiteData returned error";
  UMA_HISTOGRAM_TIMES("ClearPluginData.time",
                      base::Time::Now() - remove_start_time_);
  SignalDone();
}

void PluginDataRemoverImpl::OnTimeout() {
  LOG_IF(ERROR, is_removing_) << "Timed out";
  SignalDone();
}

bool PluginDataRemoverImpl::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PluginDataRemoverImpl, msg)
    IPC_MESSAGE_HANDLER(PluginHostMsg_ClearSiteDataResult,
                        OnClearSiteDataResult)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  return true;
}

void PluginDataRemoverImpl::OnChannelError() {
  is_starting_process_ = false;
  if (is_removing_) {
    NOTREACHED() << "Channel error";
    SignalDone();
  }
}

void PluginDataRemoverImpl::SignalDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!is_removing_)
    return;
  is_removing_ = false;
  event_->Signal();
}
