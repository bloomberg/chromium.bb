// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_UNPRIVILEGED_H_
#define IPC_ATTACHMENT_BROKER_UNPRIVILEGED_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/attachment_broker.h"
#include "ipc/ipc_export.h"

namespace IPC {

class Endpoint;
class Sender;

// This abstract subclass of AttachmentBroker is intended for use in
// non-privileged processes.
class IPC_EXPORT AttachmentBrokerUnprivileged : public IPC::AttachmentBroker {
 public:
  AttachmentBrokerUnprivileged();
  ~AttachmentBrokerUnprivileged() override;

   // On platforms that support attachment brokering, returns a new instance of
   // a platform-specific attachment broker. Otherwise returns |nullptr|.
   // The caller takes ownership of the newly created instance, and is
   // responsible for ensuring that the attachment broker lives longer than
   // every IPC::Channel. The new instance automatically registers itself as the
   // global attachment broker.
  static scoped_ptr<AttachmentBrokerUnprivileged> CreateBroker();

  // In each unprivileged process, exactly one channel should be used to
  // communicate brokerable attachments with the broker process.
  void DesignateBrokerCommunicationChannel(Endpoint* endpoint);

 protected:
  IPC::Sender* get_sender() { return sender_; }

  // Errors that can be reported by subclasses.
  // These match tools/metrics/histograms/histograms.xml.
  // This enum is append-only.
  enum UMAError {
    // The brokerable attachment was successfully processed.
    SUCCESS = 0,
    // The brokerable attachment's destination was not the process that received
    // the attachment.
    WRONG_DESTINATION = 1,
    // An error occurred while trying to receive a Mach port with mach_msg().
    ERR_RECEIVE_MACH_MESSAGE = 2,
    ERROR_MAX
  };

  // Emits an UMA metric.
  void LogError(UMAError error);

 private:
  // |sender_| is used to send Messages to the privileged broker process.
  // |sender_| must live at least as long as this instance.
  IPC::Sender* sender_;
  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerUnprivileged);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_UNPRIVILEGED_H_
