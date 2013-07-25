// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/midi_dispatcher.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/common/media/midi_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebMIDIPermissionRequest.h"

using WebKit::WebMIDIPermissionRequest;

namespace content {

MIDIDispatcher::MIDIDispatcher(RenderViewImpl* render_view)
    : RenderViewObserver(render_view) {
}

MIDIDispatcher::~MIDIDispatcher() {}

bool MIDIDispatcher::OnMessageReceived(const IPC::Message& message) {
  // TODO(toyoshim): Handle MIDIMsg_SysExPermissionApproved.
  return false;
}

void MIDIDispatcher::requestSysExPermission(
      const WebMIDIPermissionRequest& request) {
  // TODO(toyoshim): Send a message to MIDIDispatcherHost to handle correctly.
  int client_id = requests_.Add(new WebMIDIPermissionRequest(request));
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &MIDIDispatcher::OnSysExPermissionApproved,
      base::Unretained(this), client_id, false));
}

void MIDIDispatcher::cancelSysExPermissionRequest(
    const WebMIDIPermissionRequest& request) {
  for (IDMap<WebMIDIPermissionRequest>::iterator it(&requests_);
       !it.IsAtEnd();
       it.Advance()) {
    WebMIDIPermissionRequest* value = it.GetCurrentValue();
    if (!value->equals(request))
      continue;
    requests_.Remove(it.GetCurrentKey());
  }
}

void MIDIDispatcher::OnSysExPermissionApproved(int client_id, bool is_allowed) {
  // |request| can be NULL when the request is canceled.
  WebMIDIPermissionRequest* request = requests_.Lookup(client_id);
  if (!request)
    return;
  request->setIsAllowed(is_allowed);
  requests_.Remove(client_id);
}

}  // namespace content
