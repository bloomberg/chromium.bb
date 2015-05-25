// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/midi_dispatcher.h"

#include "base/bind.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebMIDIPermissionRequest.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

using blink::WebMIDIPermissionRequest;
using blink::WebSecurityOrigin;

namespace content {

MidiDispatcher::MidiDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

MidiDispatcher::~MidiDispatcher() {}

void MidiDispatcher::requestSysexPermission(
      const WebMIDIPermissionRequest& request) {
  if (!permission_service_.get()) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&permission_service_));
  }

  int permission_request_id =
      requests_.Add(new WebMIDIPermissionRequest(request));

  permission_service_->RequestPermission(
      PERMISSION_NAME_MIDI_SYSEX,
      request.securityOrigin().toString().utf8(),
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      base::Bind(&MidiDispatcher::OnSysExPermissionSet,
                 base::Unretained(this),
                 permission_request_id));
}

void MidiDispatcher::cancelSysexPermissionRequest(
    const WebMIDIPermissionRequest& request) {
  for (Requests::iterator it(&requests_); !it.IsAtEnd(); it.Advance()) {
    WebMIDIPermissionRequest* value = it.GetCurrentValue();
    if (!value->equals(request))
      continue;
    requests_.Remove(it.GetCurrentKey());
    break;
  }
}

void MidiDispatcher::OnSysExPermissionSet(int request_id,
                                          PermissionStatus status) {
  // |request| can be NULL when the request is canceled.
  WebMIDIPermissionRequest* request = requests_.Lookup(request_id);
  if (!request)
    return;
  request->setIsAllowed(status == PERMISSION_STATUS_GRANTED);
  requests_.Remove(request_id);
}

}  // namespace content
