// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/mock_render_process_host.h"

MockRenderProcessHost::MockRenderProcessHost(Profile* profile)
    : RenderProcessHost(profile),
      transport_dib_(NULL),
      bad_msg_count_(0) {
}

MockRenderProcessHost::~MockRenderProcessHost() {
  delete transport_dib_;
}

bool MockRenderProcessHost::Init(bool is_extensions_process,
                                 URLRequestContextGetter* request_context) {
  return true;
}

int MockRenderProcessHost::GetNextRoutingID() {
  static int prev_routing_id = 0;
  return ++prev_routing_id;
}

void MockRenderProcessHost::CancelResourceRequests(int render_widget_id) {
}

void MockRenderProcessHost::CrossSiteClosePageACK(
    const ViewMsg_ClosePage_Params& params) {
}

bool MockRenderProcessHost::WaitForUpdateMsg(int render_widget_id,
                                             const base::TimeDelta& max_delay,
                                             IPC::Message* msg) {
  return false;
}

void MockRenderProcessHost::ReceivedBadMessage(uint32 msg_type) {
  ++bad_msg_count_;
}

void MockRenderProcessHost::WidgetRestored() {
}

void MockRenderProcessHost::WidgetHidden() {
}

void MockRenderProcessHost::ViewCreated() {
}

void MockRenderProcessHost::AddWord(const string16& word) {
}

void MockRenderProcessHost::SendVisitedLinkTable(
    base::SharedMemory* table_memory) {
}

void MockRenderProcessHost::AddVisitedLinks(
    const VisitedLinkCommon::Fingerprints& links) {
}

void MockRenderProcessHost::ResetVisitedLinks() {
}

bool MockRenderProcessHost::FastShutdownIfPossible() {
  // We aren't actually going to do anything, but set |fast_shutdown_started_|
  // to true so that tests know we've been called.
  fast_shutdown_started_ = true;
  return true;
}

bool MockRenderProcessHost::SendWithTimeout(IPC::Message* msg, int timeout_ms) {
  // Save the message in the sink. Just ignore timeout_ms.
  sink_.OnMessageReceived(*msg);
  delete msg;
  return true;
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
#elif defined(OS_LINUX)
  transport_dib_ = TransportDIB::Map(dib_id);
#endif

  return transport_dib_;
}

void MockRenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
}

void MockRenderProcessHost::OnChannelConnected(int32 peer_pid) {
}
