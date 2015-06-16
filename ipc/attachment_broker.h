// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_H_
#define IPC_ATTACHMENT_BROKER_H_

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_export.h"

namespace IPC {

class AttachmentBroker;
// Classes that inherit from this abstract base class are capable of
// communicating with a broker to send and receive attachments to Chrome IPC
// messages.
class IPC_EXPORT SupportsAttachmentBrokering {
 public:
  // Returns an AttachmentBroker used to broker attachments of IPC messages to
  // other processes. There must be exactly one AttachmentBroker per process.
  virtual AttachmentBroker* GetAttachmentBroker() = 0;
};

// Responsible for brokering attachments to Chrome IPC messages. On platforms
// that support attachment brokering, every IPC channel should have a reference
// to a AttachmentBroker.
class IPC_EXPORT AttachmentBroker {
 public:
  AttachmentBroker() {}
  virtual ~AttachmentBroker() {}

  // Sends |attachment| to |destination_process|. The implementation uses an
  // IPC::Channel to communicate with the broker process. This may be the same
  // IPC::Channel that is requesting the brokering of an attachment.
  virtual void SendAttachmentToProcess(BrokerableAttachment* attachment,
                                       base::ProcessId destination_process) = 0;

  // Returns whether the attachment was available. If the attachment was
  // available, populates the output parameter |attachment|. The caller then
  // becomes the owner of |attachment|.
  virtual bool GetAttachmentWithId(BrokerableAttachment::AttachmentId id,
                                   BrokerableAttachment* attachment) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AttachmentBroker);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_H_
