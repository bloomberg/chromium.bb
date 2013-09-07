// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_IPC_INPUT_EVENT_PAYLOAD_H_
#define CONTENT_COMMON_INPUT_IPC_INPUT_EVENT_PAYLOAD_H_

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event.h"
#include "ipc/ipc_message.h"

namespace content {

// Wraps a generic IPC message of type InputMsg_*.
class CONTENT_EXPORT IPCInputEventPayload : public InputEvent::Payload {
 public:
  static scoped_ptr<IPCInputEventPayload> Create();
  static scoped_ptr<IPCInputEventPayload> Create(
      scoped_ptr<IPC::Message> message);

  static const IPCInputEventPayload* Cast(const Payload* payload);

  virtual ~IPCInputEventPayload();

  // InputEvent::Payload
  virtual Type GetType() const OVERRIDE;

  scoped_ptr<IPC::Message> message;

 protected:
  IPCInputEventPayload();

 private:
  DISALLOW_COPY_AND_ASSIGN(IPCInputEventPayload);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_IPC_INPUT_EVENT_PAYLOAD_H_
