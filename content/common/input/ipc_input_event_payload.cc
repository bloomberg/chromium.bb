// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/ipc_input_event_payload.h"

namespace content {

scoped_ptr<IPCInputEventPayload> IPCInputEventPayload::Create() {
  return make_scoped_ptr(new IPCInputEventPayload());
}

scoped_ptr<IPCInputEventPayload> IPCInputEventPayload::Create(
    scoped_ptr<IPC::Message> message) {
  DCHECK(message);
  scoped_ptr<IPCInputEventPayload> payload = Create();
  payload->message = message.Pass();
  return payload.Pass();
}

const IPCInputEventPayload* IPCInputEventPayload::Cast(const Payload* payload) {
  DCHECK(payload);
  DCHECK_EQ(IPC_MESSAGE, payload->GetType());
  return static_cast<const IPCInputEventPayload*>(payload);
}

IPCInputEventPayload::IPCInputEventPayload() {}

IPCInputEventPayload::~IPCInputEventPayload() {}

InputEvent::Payload::Type IPCInputEventPayload::GetType() const {
  return IPC_MESSAGE;
}

}  // namespace content
