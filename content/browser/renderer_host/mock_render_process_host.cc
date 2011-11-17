// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/mock_render_process_host.h"

#include "content/browser/child_process_security_policy.h"

MockRenderProcessHost::MockRenderProcessHost(
    content::BrowserContext* browser_context)
        : RenderProcessHost(browser_context),
          transport_dib_(NULL),
          bad_msg_count_(0),
          factory_(NULL) {
  // Child process security operations can't be unit tested unless we add
  // ourselves as an existing child process.
  ChildProcessSecurityPolicy::GetInstance()->Add(id());
}

MockRenderProcessHost::~MockRenderProcessHost() {
  ChildProcessSecurityPolicy::GetInstance()->Remove(id());
  delete transport_dib_;
  if (factory_)
    factory_->Remove(this);
}

void MockRenderProcessHost::EnableSendQueue() {
}

bool MockRenderProcessHost::Init(bool is_accessibility_enabled) {
  return true;
}

int MockRenderProcessHost::GetNextRoutingID() {
  static int prev_routing_id = 0;
  return ++prev_routing_id;
}

void MockRenderProcessHost::UpdateAndSendMaxPageID(int32 page_id) {
  UpdateMaxPageID(page_id);
}

void MockRenderProcessHost::CancelResourceRequests(int render_widget_id) {
}

void MockRenderProcessHost::CrossSiteSwapOutACK(
    const ViewMsg_SwapOut_Params& params) {
}

bool MockRenderProcessHost::WaitForUpdateMsg(int render_widget_id,
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

void MockRenderProcessHost::AddWord(const string16& word) {
}


bool MockRenderProcessHost::FastShutdownIfPossible() {
  // We aren't actually going to do anything, but set |fast_shutdown_started_|
  // to true so that tests know we've been called.
  fast_shutdown_started_ = true;
  return true;
}

void MockRenderProcessHost::DumpHandles() {
}

base::ProcessHandle MockRenderProcessHost::GetHandle() {
  return base::kNullProcessHandle;
}

bool MockRenderProcessHost::Send(IPC::Message* msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(*msg);
  delete msg;
  return true;
}

TransportDIB* MockRenderProcessHost::GetTransportDIB(TransportDIB::Id dib_id) {
  if (transport_dib_)
    return transport_dib_;
#if defined(OS_WIN)
  HANDLE duped;
  DuplicateHandle(GetCurrentProcess(), dib_id.handle, GetCurrentProcess(),
                  &duped, 0, TRUE, DUPLICATE_SAME_ACCESS);
  transport_dib_ = TransportDIB::Map(duped);
#elif defined(OS_MACOSX)
  // On Mac, TransportDIBs are always created in the browser, so we cannot map
  // one from a dib_id.
  transport_dib_ = TransportDIB::Create(100 * 100 * 4, 0);
#elif defined(OS_POSIX)
  transport_dib_ = TransportDIB::Map(dib_id.shmkey);
#endif

  return transport_dib_;
}

void MockRenderProcessHost::SetCompositingSurface(
    int render_widget_id,
    gfx::PluginWindowHandle compositing_surface) {
}

bool MockRenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
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
    content::BrowserContext* browser_context) const {
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
