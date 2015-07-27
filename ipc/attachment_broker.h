// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_H_
#define IPC_ATTACHMENT_BROKER_H_

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_listener.h"

// If the platform has no attachments that need brokering, then it shouldn't
// compile any code that calls member functions of AttachmentBroker. This
// prevents symbols only used by AttachmentBroker and its subclasses from
// making it into the binary.
#if defined(OS_WIN)
#define USE_ATTACHMENT_BROKER 1
#else
#define USE_ATTACHMENT_BROKER 0
#endif  // defined(OS_WIN)

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
// This class is not thread safe. The implementation of this class assumes that
// it is only ever used on the same thread as its consumers.
class IPC_EXPORT AttachmentBroker : public Listener {
 public:
  // A standard observer interface that allows consumers of the AttachmentBroker
  // to be notified when a new attachment has been received.
  class Observer {
   public:
    virtual void ReceivedBrokerableAttachmentWithId(
        const BrokerableAttachment::AttachmentId& id) = 0;
  };

  AttachmentBroker();
  ~AttachmentBroker() override;

  // Sends |attachment| to |destination_process|. The implementation uses an
  // IPC::Channel to communicate with the broker process. This may be the same
  // IPC::Channel that is requesting the brokering of an attachment.
  // Returns true on success and false otherwise.
  virtual bool SendAttachmentToProcess(const BrokerableAttachment* attachment,
                                       base::ProcessId destination_process) = 0;

  // Returns whether the attachment was available. If the attachment was
  // available, populates the output parameter |attachment|.
  virtual bool GetAttachmentWithId(
      BrokerableAttachment::AttachmentId id,
      scoped_refptr<BrokerableAttachment>* attachment) = 0;

  // Any given observer should only ever add itself once to the observer list.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyObservers(const BrokerableAttachment::AttachmentId& id);

 private:
  DISALLOW_COPY_AND_ASSIGN(AttachmentBroker);

  std::vector<Observer*> observers_;
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_H_
