// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_data_remover_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/waitable_event.h"
#include "base/version.h"
#include "content/browser/plugin_process_host.h"
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

class PluginDataRemoverImpl::Context
    : public PluginProcessHost::Client,
      public IPC::Channel::Listener,
      public base::RefCountedThreadSafe<Context> {
 public:
  Context(const std::string& mime_type,
          base::Time begin_time,
          const content::ResourceContext& resource_context)
      : event_(new base::WaitableEvent(true, false)),
        begin_time_(begin_time),
        is_removing_(false),
        resource_context_(resource_context),
        channel_(NULL) {
    // Balanced in OnChannelOpened or OnError. Exactly one them will eventually
    // be called, so we need to keep this object around until then.
    AddRef();
    remove_start_time_ = base::Time::Now();
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&Context::Init, this, mime_type));

    BrowserThread::PostDelayedTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&Context::OnTimeout, this),
        kRemovalTimeoutMs);
  }

  virtual ~Context() {
    if (channel_)
      BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, channel_);
  }

  // PluginProcessHost::Client methods.
  virtual int ID() OVERRIDE {
    // Generate a unique identifier for this PluginProcessHostClient.
    return ChildProcessHost::GenerateChildProcessUniqueId();
  }

  virtual bool OffTheRecord() OVERRIDE {
    return false;
  }

  virtual const content::ResourceContext& GetResourceContext() OVERRIDE {
    return resource_context_;
  }

  virtual void SetPluginInfo(const webkit::WebPluginInfo& info) OVERRIDE {
  }

  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE {
  }

  virtual void OnSentPluginChannelRequest() OVERRIDE {
  }

  virtual void OnChannelOpened(const IPC::ChannelHandle& handle) OVERRIDE {
    ConnectToChannel(handle);
    // Balancing the AddRef call.
    Release();
  }

  virtual void OnError() OVERRIDE {
    LOG(DFATAL) << "Couldn't open plugin channel";
    SignalDone();
    // Balancing the AddRef call.
    Release();
  }

  // IPC::Channel::Listener methods.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    IPC_BEGIN_MESSAGE_MAP(Context, message)
      IPC_MESSAGE_HANDLER(PluginHostMsg_ClearSiteDataResult,
                          OnClearSiteDataResult)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP()

    return true;
  }

  virtual void OnChannelError() OVERRIDE {
    if (is_removing_) {
      NOTREACHED() << "Channel error";
      SignalDone();
    }
  }


  base::WaitableEvent* event() { return event_.get(); }

 private:
  // Initialize on the IO thread.
  void Init(const std::string& mime_type) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    is_removing_ = true;
    PluginService::GetInstance()->OpenChannelToNpapiPlugin(
        0, 0, GURL(), GURL(), mime_type, this);
  }

  // Connects the client side of a newly opened plug-in channel.
  void ConnectToChannel(const IPC::ChannelHandle& handle) {
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

  // Handles the PluginHostMsg_ClearSiteDataResult message.
  void OnClearSiteDataResult(bool success) {
    LOG_IF(ERROR, !success) << "ClearSiteData returned error";
    UMA_HISTOGRAM_TIMES("ClearPluginData.time",
                        base::Time::Now() - remove_start_time_);
    SignalDone();
  }

  // Called when a timeout happens in order not to block the client
  // indefinitely.
  void OnTimeout() {
    LOG_IF(ERROR, is_removing_) << "Timed out";
    SignalDone();
  }

  // Signals that we are finished with removing data (successful or not). This
  // method is safe to call multiple times.
  void SignalDone() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!is_removing_)
      return;
    is_removing_ = false;
    event_->Signal();
  }

  scoped_ptr<base::WaitableEvent> event_;
  // The point in time when we start removing data.
  base::Time remove_start_time_;
  // The point in time from which on we remove data.
  base::Time begin_time_;
  bool is_removing_;

  // The resource context for the profile.
  const content::ResourceContext& resource_context_;

  // We own the channel, but it's used on the IO thread, so it needs to be
  // deleted there. It's NULL until we have opened a connection to the plug-in
  // process.
  IPC::Channel* channel_;
};


PluginDataRemoverImpl::PluginDataRemoverImpl(
    const content::ResourceContext& resource_context)
    : mime_type_(kFlashMimeType),
      resource_context_(resource_context) {
}

PluginDataRemoverImpl::~PluginDataRemoverImpl() {
}

base::WaitableEvent* PluginDataRemoverImpl::StartRemoving(
    base::Time begin_time) {
  DCHECK(!context_.get());
  context_ = new Context(mime_type_, begin_time, resource_context_);
  return context_->event();
}
