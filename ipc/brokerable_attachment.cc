// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/brokerable_attachment.h"

#include "crypto/random.h"

namespace IPC {

namespace {

// In order to prevent mutually untrusted processes from stealing resources from
// one another, the nonce must be secret. This generates a 128-bit,
// cryptographicaly-strong random number.
BrokerableAttachment::AttachmentId GetRandomId() {
  BrokerableAttachment::AttachmentId id;
  crypto::RandBytes(id.nonce, BrokerableAttachment::kNonceSize);
  return id;
}

}  // namespace

BrokerableAttachment::BrokerableAttachment() : id_(GetRandomId()) {
}

BrokerableAttachment::~BrokerableAttachment() {
}

BrokerableAttachment::AttachmentId BrokerableAttachment::GetIdentifier() const {
  return id_;
}

BrokerableAttachment::Type BrokerableAttachment::GetType() const {
  return TYPE_BROKERABLE_ATTACHMENT;
}

}  // namespace IPC
