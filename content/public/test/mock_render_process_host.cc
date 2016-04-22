// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_render_process_host.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/storage_partition.h"

#if defined(ENABLE_BROWSER_CDMS)
#include "media/base/media_keys.h"
#endif

namespace content {

MockRenderProcessHost::MockRenderProcessHost(BrowserContext* browser_context)
    : bad_msg_count_(0),
      factory_(NULL),
      id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      has_connection_(false),
      browser_context_(browser_context),
      prev_routing_id_(0),
      fast_shutdown_started_(false),
      deletion_callback_called_(false),
      is_for_guests_only_(false),
      is_process_backgrounded_(false),
      worker_ref_count_(0) {
  // Child process security operations can't be unit tested unless we add
  // ourselves as an existing child process.
  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID());

  RenderProcessHostImpl::RegisterHost(GetID(), this);
}

MockRenderProcessHost::~MockRenderProcessHost() {
  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(GetID());
  if (factory_)
    factory_->Remove(this);

  // In unit tests, Cleanup() might not have been called.
  if (!deletion_callback_called_) {
    FOR_EACH_OBSERVER(RenderProcessHostObserver,
                      observers_,
                      RenderProcessHostDestroyed(this));
    RenderProcessHostImpl::UnregisterHost(GetID());
  }
}

void MockRenderProcessHost::SimulateCrash() {
  has_connection_ = false;
  RenderProcessHost::RendererClosedDetails details(
      base::TERMINATION_STATUS_PROCESS_CRASHED, 0);
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_CLOSED, Source<RenderProcessHost>(this),
      Details<RenderProcessHost::RendererClosedDetails>(&details));

  FOR_EACH_OBSERVER(
      RenderProcessHostObserver, observers_,
      RenderProcessExited(this, details.status, details.exit_code));

  // Send every routing ID a FrameHostMsg_RenderProcessGone message. To ensure a
  // predictable order for unittests which may assert against the order, we sort
  // the listeners by descending routing ID, instead of using the arbitrary
  // hash-map order like RenderProcessHostImpl.
  std::vector<std::pair<int32_t, IPC::Listener*>> sorted_listeners_;
  IDMap<IPC::Listener>::iterator iter(&listeners_);
  while (!iter.IsAtEnd()) {
    sorted_listeners_.push_back(
        std::make_pair(iter.GetCurrentKey(), iter.GetCurrentValue()));
    iter.Advance();
  }
  std::sort(sorted_listeners_.rbegin(), sorted_listeners_.rend());

  for (auto& entry_pair : sorted_listeners_) {
    entry_pair.second->OnMessageReceived(FrameHostMsg_RenderProcessGone(
        entry_pair.first, static_cast<int>(details.status), details.exit_code));
  }
}

void MockRenderProcessHost::EnableSendQueue() {
}

bool MockRenderProcessHost::Init() {
  has_connection_ = true;
  return true;
}

int MockRenderProcessHost::GetNextRoutingID() {
  return ++prev_routing_id_;
}

void MockRenderProcessHost::AddRoute(int32_t routing_id,
                                     IPC::Listener* listener) {
  listeners_.AddWithID(listener, routing_id);
}

void MockRenderProcessHost::RemoveRoute(int32_t routing_id) {
  DCHECK(listeners_.Lookup(routing_id) != NULL);
  listeners_.Remove(routing_id);
  Cleanup();
}

void MockRenderProcessHost::AddObserver(RenderProcessHostObserver* observer) {
  observers_.AddObserver(observer);
}

void MockRenderProcessHost::RemoveObserver(
    RenderProcessHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MockRenderProcessHost::ShutdownForBadMessage() {
  ++bad_msg_count_;
}

void MockRenderProcessHost::WidgetRestored() {
}

void MockRenderProcessHost::WidgetHidden() {
}

int MockRenderProcessHost::VisibleWidgetCount() const {
  return 1;
}

void MockRenderProcessHost::AudioStateChanged() {}

bool MockRenderProcessHost::IsForGuestsOnly() const {
  return is_for_guests_only_;
}

StoragePartition* MockRenderProcessHost::GetStoragePartition() const {
  return BrowserContext::GetDefaultStoragePartition(browser_context_);
}

void MockRenderProcessHost::AddWord(const base::string16& word) {
}

bool MockRenderProcessHost::Shutdown(int exit_code, bool wait) {
  return true;
}

bool MockRenderProcessHost::FastShutdownIfPossible() {
  // We aren't actually going to do anything, but set |fast_shutdown_started_|
  // to true so that tests know we've been called.
  fast_shutdown_started_ = true;
  return true;
}

bool MockRenderProcessHost::FastShutdownStarted() const {
  return fast_shutdown_started_;
}

base::ProcessHandle MockRenderProcessHost::GetHandle() const {
  // Return the current-process handle for the IPC::GetPlatformFileForTransit
  // function.
  if (process_handle)
    return *process_handle;
  return base::GetCurrentProcessHandle();
}

bool MockRenderProcessHost::IsReady() const {
  return false;
}

bool MockRenderProcessHost::Send(IPC::Message* msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(*msg);
  delete msg;
  return true;
}

int MockRenderProcessHost::GetID() const {
  return id_;
}

bool MockRenderProcessHost::HasConnection() const {
  return has_connection_;
}

void MockRenderProcessHost::SetIgnoreInputEvents(bool ignore_input_events) {
}

bool MockRenderProcessHost::IgnoreInputEvents() const {
  return false;
}

void MockRenderProcessHost::Cleanup() {
  if (listeners_.IsEmpty()) {
    FOR_EACH_OBSERVER(RenderProcessHostObserver,
                      observers_,
                      RenderProcessHostDestroyed(this));
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    RenderProcessHostImpl::UnregisterHost(GetID());
    deletion_callback_called_ = true;
  }
}

void MockRenderProcessHost::AddPendingView() {
}

void MockRenderProcessHost::RemovePendingView() {
}

void MockRenderProcessHost::SetSuddenTerminationAllowed(bool allowed) {
}

bool MockRenderProcessHost::SuddenTerminationAllowed() const {
  return true;
}

BrowserContext* MockRenderProcessHost::GetBrowserContext() const {
  return browser_context_;
}

bool MockRenderProcessHost::InSameStoragePartition(
    StoragePartition* partition) const {
  // Mock RPHs only have one partition.
  return true;
}

IPC::ChannelProxy* MockRenderProcessHost::GetChannel() {
  return NULL;
}

void MockRenderProcessHost::AddFilter(BrowserMessageFilter* filter) {
}

bool MockRenderProcessHost::FastShutdownForPageCount(size_t count) {
  if (GetActiveViewCount() == count)
    return FastShutdownIfPossible();
  return false;
}

base::TimeDelta MockRenderProcessHost::GetChildProcessIdleTime() const {
  return base::TimeDelta::FromMilliseconds(0);
}

void MockRenderProcessHost::NotifyTimezoneChange(const std::string& zone_id) {
}

ServiceRegistry* MockRenderProcessHost::GetServiceRegistry() {
  return service_registry_.get();
}

shell::Connection* MockRenderProcessHost::GetChildConnection() {
  return nullptr;
}

std::unique_ptr<base::SharedPersistentMemoryAllocator>
MockRenderProcessHost::TakeMetricsAllocator() {
  return nullptr;
}

const base::TimeTicks& MockRenderProcessHost::GetInitTimeForNavigationMetrics()
    const {
  static base::TimeTicks dummy_time = base::TimeTicks::Now();
  return dummy_time;
}

#if defined(ENABLE_BROWSER_CDMS)
scoped_refptr<media::MediaKeys> MockRenderProcessHost::GetCdm(
    int render_frame_id,
    int cdm_id) const {
  return nullptr;
}
#endif

bool MockRenderProcessHost::IsProcessBackgrounded() const {
  return is_process_backgrounded_;
}

void MockRenderProcessHost::IncrementWorkerRefCount() {
  ++worker_ref_count_;
}

void MockRenderProcessHost::DecrementWorkerRefCount() {
  --worker_ref_count_;
}

void MockRenderProcessHost::FilterURL(bool empty_allowed, GURL* url) {
  RenderProcessHostImpl::FilterURL(this, empty_allowed, url);
}

#if defined(ENABLE_WEBRTC)
void MockRenderProcessHost::EnableAudioDebugRecordings(
    const base::FilePath& file) {
}

void MockRenderProcessHost::DisableAudioDebugRecordings() {}

void MockRenderProcessHost::EnableEventLogRecordings(
    const base::FilePath& file) {}

void MockRenderProcessHost::DisableEventLogRecordings() {}

void MockRenderProcessHost::SetWebRtcLogMessageCallback(
    base::Callback<void(const std::string&)> callback) {
}

void MockRenderProcessHost::ClearWebRtcLogMessageCallback() {}

RenderProcessHost::WebRtcStopRtpDumpCallback
MockRenderProcessHost::StartRtpDump(
    bool incoming,
    bool outgoing,
    const WebRtcRtpPacketCallback& packet_callback) {
  return WebRtcStopRtpDumpCallback();
}
#endif

void MockRenderProcessHost::ResumeDeferredNavigation(
    const GlobalRequestID& request_id) {}

bool MockRenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
  IPC::Listener* listener = listeners_.Lookup(msg.routing_id());
  if (listener)
    return listener->OnMessageReceived(msg);
  return false;
}

void MockRenderProcessHost::OnChannelConnected(int32_t peer_pid) {}

MockRenderProcessHostFactory::MockRenderProcessHostFactory() {}

MockRenderProcessHostFactory::~MockRenderProcessHostFactory() {
  // Detach this object from MockRenderProcesses to prevent STLDeleteElements()
  // from calling MockRenderProcessHostFactory::Remove().
  for (ScopedVector<MockRenderProcessHost>::iterator it = processes_.begin();
       it != processes_.end(); ++it) {
    (*it)->SetFactory(NULL);
  }
}

RenderProcessHost* MockRenderProcessHostFactory::CreateRenderProcessHost(
    BrowserContext* browser_context,
    SiteInstance* site_instance) const {
  MockRenderProcessHost* host = new MockRenderProcessHost(browser_context);
  if (host) {
    processes_.push_back(host);
    host->SetFactory(this);
  }
  return host;
}

void MockRenderProcessHostFactory::Remove(MockRenderProcessHost* host) const {
  for (ScopedVector<MockRenderProcessHost>::iterator it = processes_.begin();
       it != processes_.end(); ++it) {
    if (*it == host) {
      processes_.weak_erase(it);
      break;
    }
  }
}

}  // namespace content
