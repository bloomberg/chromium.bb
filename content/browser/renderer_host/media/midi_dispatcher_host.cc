// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/midi_dispatcher_host.h"

#include "base/bind.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/media/midi_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace content {

MIDIDispatcherHost::MIDIDispatcherHost(int render_process_id,
                                       BrowserContext* browser_context)
    : render_process_id_(render_process_id),
      browser_context_(browser_context) {
}

MIDIDispatcherHost::~MIDIDispatcherHost() {
}

bool MIDIDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                           bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MIDIDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(MIDIHostMsg_RequestSysExPermission,
                        OnRequestSysExPermission)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void MIDIDispatcherHost::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  if (message.type() == MIDIHostMsg_RequestSysExPermission::ID)
    *thread = BrowserThread::UI;
}

void MIDIDispatcherHost::OnRequestSysExPermission(int render_view_id,
                                                  int client_id,
                                                  const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  browser_context_->RequestMIDISysExPermission(
      render_process_id_,
      render_view_id,
      origin,
      base::Bind(&MIDIDispatcherHost::WasSysExPermissionGranted,
                 base::Unretained(this),
                 render_view_id,
                 client_id));
}

void MIDIDispatcherHost::WasSysExPermissionGranted(int render_view_id,
                                                   int client_id,
                                                   bool success) {
  Send(new MIDIMsg_SysExPermissionApproved(render_view_id, client_id, success));
}

}  // namespace content
