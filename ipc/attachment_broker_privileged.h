// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_PRIVILEGED_H_
#define IPC_ATTACHMENT_BROKER_PRIVILEGED_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "ipc/attachment_broker.h"
#include "ipc/ipc_export.h"

namespace IPC {

class Endpoint;
class Sender;

// This abstract subclass of AttachmentBroker is intended for use in a
// privileged process . When unprivileged processes want to send attachments,
// the attachments get routed through the privileged process, and more
// specifically, an instance of this class.
class IPC_EXPORT AttachmentBrokerPrivileged : public IPC::AttachmentBroker {
 public:
  AttachmentBrokerPrivileged();
  ~AttachmentBrokerPrivileged() override;

   // On platforms that support attachment brokering, returns a new instance of
   // a platform-specific attachment broker. Otherwise returns |nullptr|.
   // The caller takes ownership of the newly created instance, and is
   // responsible for ensuring that the attachment broker lives longer than
   // every IPC::Channel. The new instance automatically registers itself as the
   // global attachment broker.
  static scoped_ptr<AttachmentBrokerPrivileged> CreateBroker();

  // AttachmentBroker overrides.
  void RegisterCommunicationChannel(Endpoint* endpoint) override;
  void DeregisterCommunicationChannel(Endpoint* endpoint) override;

 protected:
  // Returns the sender whose peer's process id is |id|.
  // Returns nullptr if no sender is found.
  Sender* GetSenderWithProcessId(base::ProcessId id);

  // Errors that can be reported by subclasses.
  // These match tools/metrics/histograms.xml.
  // This enum is append-only.
  enum UMAError {
    // The brokerable attachment had a valid destination. This is the success
    // case.
    DESTINATION_FOUND = 0,
    // The brokerable attachment had a destination, but the broker did not have
    // a channel of communication with that process.
    DESTINATION_NOT_FOUND = 1,
    // The brokerable attachment did not have a destination process.
    NO_DESTINATION = 2,
    ERROR_MAX
  };

  // Emits an UMA metric.
  void LogError(UMAError error);

 private:
  std::vector<Endpoint*> endpoints_;
  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerPrivileged);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_PRIVILEGED_H_
