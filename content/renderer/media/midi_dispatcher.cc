// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/midi_dispatcher.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/common/media/midi_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebMIDIPermissionRequest.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

using blink::WebMIDIPermissionRequest;
using blink::WebSecurityOrigin;

namespace content {

MidiDispatcher::MidiDispatcher(RenderViewImpl* render_view)
    : RenderViewObserver(render_view) {
}

MidiDispatcher::~MidiDispatcher() {}

bool MidiDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MidiDispatcher, message)
    IPC_MESSAGE_HANDLER(MidiMsg_SysExPermissionApproved,
                        OnSysExPermissionApproved)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MidiDispatcher::requestSysExPermission(
      const WebMIDIPermissionRequest& request) {
  int bridge_id = requests_.Add(new WebMIDIPermissionRequest(request));
  WebSecurityOrigin security_origin = request.securityOrigin();
  std::string origin = security_origin.toString().utf8();
  GURL url(origin);
  Send(new MidiHostMsg_RequestSysExPermission(routing_id(), bridge_id, url));
}

void MidiDispatcher::cancelSysExPermissionRequest(
    const WebMIDIPermissionRequest& request) {
  for (IDMap<WebMIDIPermissionRequest>::iterator it(&requests_);
       !it.IsAtEnd();
       it.Advance()) {
    WebMIDIPermissionRequest* value = it.GetCurrentValue();
    if (value->equals(request)) {
      base::string16 origin = request.securityOrigin().toString();
      Send(new MidiHostMsg_CancelSysExPermissionRequest(
          routing_id(), it.GetCurrentKey(), GURL(origin)));
      requests_.Remove(it.GetCurrentKey());
      break;
    }
  }
}

void MidiDispatcher::OnSysExPermissionApproved(int bridge_id,
                                               bool is_allowed) {
  // |request| can be NULL when the request is canceled.
  WebMIDIPermissionRequest* request = requests_.Lookup(bridge_id);
  if (!request)
    return;
  request->setIsAllowed(is_allowed);
  requests_.Remove(bridge_id);
}

}  // namespace content
