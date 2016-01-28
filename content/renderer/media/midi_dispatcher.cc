// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/midi_dispatcher.h"

#include "base/bind.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/modules/webmidi/WebMIDIOptions.h"
#include "third_party/WebKit/public/web/modules/webmidi/WebMIDIPermissionRequest.h"

using blink::WebMIDIPermissionRequest;
using blink::WebMIDIOptions;
using blink::WebSecurityOrigin;

namespace content {

MidiDispatcher::MidiDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

MidiDispatcher::~MidiDispatcher() {}

void MidiDispatcher::requestPermission(const WebMIDIPermissionRequest& request,
                                       const WebMIDIOptions& options) {
  if (options.sysex == WebMIDIOptions::SysexPermission::WithoutSysex)
    return WebMIDIPermissionRequest(request).setIsAllowed(true);

  if (!permission_service_.get()) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&permission_service_));
  }

  int permission_request_id =
      requests_.Add(new WebMIDIPermissionRequest(request));

  permission_service_->RequestPermission(
      PermissionName::MIDI_SYSEX, request.securityOrigin().toString().utf8(),
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      base::Bind(&MidiDispatcher::OnPermissionSet, base::Unretained(this),
                 permission_request_id));
}

void MidiDispatcher::cancelPermissionRequest(
    const WebMIDIPermissionRequest& request) {
  for (Requests::iterator it(&requests_); !it.IsAtEnd(); it.Advance()) {
    WebMIDIPermissionRequest* value = it.GetCurrentValue();
    if (!value->equals(request))
      continue;
    requests_.Remove(it.GetCurrentKey());
    break;
  }
}

void MidiDispatcher::OnPermissionSet(int request_id, PermissionStatus status) {
  // |request| can be NULL when the request is canceled.
  WebMIDIPermissionRequest* request = requests_.Lookup(request_id);
  if (!request)
    return;
  request->setIsAllowed(status == PermissionStatus::GRANTED);
  requests_.Remove(request_id);
}

}  // namespace content
