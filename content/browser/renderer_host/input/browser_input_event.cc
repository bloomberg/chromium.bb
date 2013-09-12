// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/browser_input_event.h"

#include "content/common/input/web_input_event_payload.h"

namespace content {

BrowserInputEvent::BrowserInputEvent(BrowserInputEventClient* client)
    : client_(client) {}

BrowserInputEvent::~BrowserInputEvent() {}

scoped_ptr<BrowserInputEvent> BrowserInputEvent::Create(
    int64 id,
    scoped_ptr<InputEvent::Payload> payload,
    BrowserInputEventClient* client) {
  DCHECK(client);
  scoped_ptr<BrowserInputEvent> event(new BrowserInputEvent(client));
  event->Initialize(id, payload.Pass());
  return event.Pass();
}

void BrowserInputEvent::OnDispatched(
    InputEventDisposition disposition,
    ScopedVector<BrowserInputEvent>* followup_events) {
  DCHECK(followup_events);

  bool event_consumed =
      disposition == INPUT_EVENT_MAIN_THREAD_CONSUMED ||
      disposition == INPUT_EVENT_IMPL_THREAD_CONSUMED ||
      disposition == INPUT_EVENT_MAIN_THREAD_PREVENT_DEFAULTED;

  if (!event_consumed && CanCreateFollowupEvents())
    client_->OnDispatched(*this, disposition, followup_events);
  else
    client_->OnDispatched(*this, disposition);
}

bool BrowserInputEvent::CanCreateFollowupEvents() const {
  return payload()->GetType() == InputEvent::Payload::WEB_INPUT_EVENT &&
         WebInputEventPayload::Cast(payload())->CanCreateFollowupEvents();
}

}  // namespace content
