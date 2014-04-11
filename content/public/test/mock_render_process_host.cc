// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_render_process_host.h"

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/storage_partition.h"

namespace content {

MockRenderProcessHost::MockRenderProcessHost(BrowserContext* browser_context)
    : transport_dib_(NULL),
      bad_msg_count_(0),
      factory_(NULL),
      id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      browser_context_(browser_context),
      prev_routing_id_(0),
      fast_shutdown_started_(false),
      deletion_callback_called_(false),
      is_guest_(false) {
  // Child process security operations can't be unit tested unless we add
  // ourselves as an existing child process.
  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID());

  RenderProcessHostImpl::RegisterHost(GetID(), this);
}

MockRenderProcessHost::~MockRenderProcessHost() {
  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(GetID());
  delete transport_dib_;
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

void MockRenderProcessHost::EnableSendQueue() {
}

bool MockRenderProcessHost::Init() {
  return true;
}

int MockRenderProcessHost::GetNextRoutingID() {
  return ++prev_routing_id_;
}

void MockRenderProcessHost::AddRoute(
    int32 routing_id,
    IPC::Listener* listener) {
  listeners_.AddWithID(listener, routing_id);
}

void MockRenderProcessHost::RemoveRoute(int32 routing_id) {
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

bool MockRenderProcessHost::WaitForBackingStoreMsg(
    int render_widget_id,
    const base::TimeDelta& max_delay,
    IPC::Message* msg) {
  return false;
}

void MockRenderProcessHost::ReceivedBadMessage() {
  ++bad_msg_count_;
}

void MockRenderProcessHost::WidgetRestored() {
}

void MockRenderProcessHost::WidgetHidden() {
}

int MockRenderProcessHost::VisibleWidgetCount() const {
  return 1;
}

bool MockRenderProcessHost::IsGuest() const {
  return is_guest_;
}

StoragePartition* MockRenderProcessHost::GetStoragePartition() const {
  return NULL;
}

void MockRenderProcessHost::AddWord(const base::string16& word) {
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

void MockRenderProcessHost::DumpHandles() {
}

base::ProcessHandle MockRenderProcessHost::GetHandle() const {
  // Return the current-process handle for the IPC::GetFileHandleForProcess
  // function.
  return base::Process::Current().handle();
}

bool MockRenderProcessHost::Send(IPC::Message* msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(*msg);
  delete msg;
  return true;
}

TransportDIB* MockRenderProcessHost::MapTransportDIB(TransportDIB::Id dib_id) {
#if defined(OS_WIN)
  // NULL should be used here instead of INVALID_HANDLE_VALUE (or pseudo-handle)
  // except for when dealing with the small number of Win16-derived APIs
  // that require INVALID_HANDLE_VALUE (e.g. CreateFile and friends).
  HANDLE duped = NULL;
  if (!DuplicateHandle(GetCurrentProcess(), dib_id.handle, GetCurrentProcess(),
                       &duped, 0, TRUE, DUPLICATE_SAME_ACCESS))
    duped = NULL;
  return TransportDIB::Map(duped);
#elif defined(OS_ANDROID)
  // On Android, Handles and Ids are the same underlying type.
  return TransportDIB::Map(dib_id);
#else
  // On POSIX, TransportDIBs are always created in the browser, so we cannot map
  // one from a dib_id.
  return TransportDIB::Create(100 * 100 * 4, 0);
#endif
}

TransportDIB* MockRenderProcessHost::GetTransportDIB(TransportDIB::Id dib_id) {
  if (transport_dib_)
    return transport_dib_;

  transport_dib_ = MapTransportDIB(dib_id);
  return transport_dib_;
}

int MockRenderProcessHost::GetID() const {
  return id_;
}

bool MockRenderProcessHost::HasConnection() const {
  return true;
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

int MockRenderProcessHost::GetActiveViewCount() {
  int num_active_views = 0;
  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Count only RenderWidgetHosts in this process.
    if (widget->GetProcess()->GetID() == GetID())
      num_active_views++;
  }
  return num_active_views;
}

bool MockRenderProcessHost::FastShutdownForPageCount(size_t count) {
  if (static_cast<size_t>(GetActiveViewCount()) == count)
    return FastShutdownIfPossible();
  return false;
}

base::TimeDelta MockRenderProcessHost::GetChildProcessIdleTime() const {
  return base::TimeDelta::FromMilliseconds(0);
}

void MockRenderProcessHost::ResumeRequestsForView(int route_id) {
}

void MockRenderProcessHost::NotifyTimezoneChange() {
}

void MockRenderProcessHost::FilterURL(bool empty_allowed, GURL* url) {
  RenderProcessHostImpl::FilterURL(this, empty_allowed, url);
}

#if defined(ENABLE_WEBRTC)
void MockRenderProcessHost::EnableAecDump(const base::FilePath& file) {
}

void MockRenderProcessHost::DisableAecDump() {
}

void MockRenderProcessHost::SetWebRtcLogMessageCallback(
    base::Callback<void(const std::string&)> callback) {
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

void MockRenderProcessHost::OnChannelConnected(int32 peer_pid) {
}

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

}  // content
