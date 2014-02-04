// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/midi_dispatcher_host.h"

#include "base/bind.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/media/midi_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace content {

MidiDispatcherHost::MidiDispatcherHost(int render_process_id,
                                       BrowserContext* browser_context)
    : render_process_id_(render_process_id),
      browser_context_(browser_context) {
}

MidiDispatcherHost::~MidiDispatcherHost() {
}

bool MidiDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                           bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MidiDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(MidiHostMsg_RequestSysExPermission,
                        OnRequestSysExPermission)
    IPC_MESSAGE_HANDLER(MidiHostMsg_CancelSysExPermissionRequest,
                        OnCancelSysExPermissionRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void MidiDispatcherHost::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == MidiMsgStart)
    *thread = BrowserThread::UI;
}

void MidiDispatcherHost::OnRequestSysExPermission(int render_view_id,
                                                  int bridge_id,
                                                  const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  browser_context_->RequestMidiSysExPermission(
      render_process_id_,
      render_view_id,
      bridge_id,
      origin,
      base::Bind(&MidiDispatcherHost::WasSysExPermissionGranted,
                 base::Unretained(this),
                 render_view_id,
                 bridge_id));
}

void MidiDispatcherHost::OnCancelSysExPermissionRequest(
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":" << render_view_id
           << ":" << bridge_id;
  browser_context_->CancelMidiSysExPermissionRequest(
      render_process_id_, render_view_id, bridge_id, requesting_frame);
}
void MidiDispatcherHost::WasSysExPermissionGranted(int render_view_id,
                                                   int bridge_id,
                                                   bool success) {
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantSendMidiSysExMessage(
      render_process_id_);
  Send(new MidiMsg_SysExPermissionApproved(render_view_id, bridge_id, success));
}

}  // namespace content
