// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/brokerable_attachment.h"

#include "ipc/attachment_broker.h"

#if USE_ATTACHMENT_BROKER
#include "crypto/random.h"
#endif

namespace IPC {

#if USE_ATTACHMENT_BROKER
BrokerableAttachment::AttachmentId::AttachmentId() {
  // In order to prevent mutually untrusted processes from stealing resources
  // from one another, the nonce must be secret. This generates a 128-bit,
  // cryptographicaly-strong random number.
  crypto::RandBytes(nonce, BrokerableAttachment::kNonceSize);
}
#else
BrokerableAttachment::AttachmentId::AttachmentId() {
  CHECK(false) << "Not allowed to construct an attachment id if the platform "
                  "does not support attachment brokering.";
}
#endif

BrokerableAttachment::AttachmentId::AttachmentId(const char* start_address,
                                                 size_t size) {
  DCHECK(size == BrokerableAttachment::kNonceSize);
  for (size_t i = 0; i < BrokerableAttachment::kNonceSize; ++i)
    nonce[i] = start_address[i];
}

void BrokerableAttachment::AttachmentId::SerializeToBuffer(char* start_address,
                                                           size_t size) {
  DCHECK(size == BrokerableAttachment::kNonceSize);
  for (size_t i = 0; i < BrokerableAttachment::kNonceSize; ++i)
    start_address[i] = nonce[i];
}

BrokerableAttachment::BrokerableAttachment() {}

BrokerableAttachment::BrokerableAttachment(const AttachmentId& id) : id_(id) {}

BrokerableAttachment::~BrokerableAttachment() {}

BrokerableAttachment::AttachmentId BrokerableAttachment::GetIdentifier() const {
  return id_;
}

bool BrokerableAttachment::NeedsBrokering() const {
  return GetBrokerableType() == PLACEHOLDER;
}

BrokerableAttachment::Type BrokerableAttachment::GetType() const {
  return TYPE_BROKERABLE_ATTACHMENT;
}

#if defined(OS_POSIX)
base::PlatformFile BrokerableAttachment::TakePlatformFile() {
  NOTREACHED();
  return base::PlatformFile();
}
#endif  // OS_POSIX

}  // namespace IPC
