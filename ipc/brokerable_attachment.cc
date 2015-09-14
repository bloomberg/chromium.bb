// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/brokerable_attachment.h"

#include "ipc/attachment_broker.h"

namespace IPC {

BrokerableAttachment::BrokerableAttachment(const AttachmentId& id,
                                           bool needs_brokering)
    : id_(id), needs_brokering_(needs_brokering) {}

BrokerableAttachment::~BrokerableAttachment() {
}

BrokerableAttachment::AttachmentId BrokerableAttachment::GetIdentifier() const {
  return id_;
}

bool BrokerableAttachment::NeedsBrokering() const {
  return needs_brokering_;
}

void BrokerableAttachment::SetNeedsBrokering(bool needs_brokering) {
  needs_brokering_ = needs_brokering;
}

BrokerableAttachment::Type BrokerableAttachment::GetType() const {
  return TYPE_BROKERABLE_ATTACHMENT;
}

}  // namespace IPC
