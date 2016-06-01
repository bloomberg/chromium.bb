// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/mailbox_struct_traits.h"

namespace mojo {

// static
bool ArrayTraits<MailboxName>::IsNull(const MailboxName& b) {
  return b.data == nullptr;
}

// static
void ArrayTraits<MailboxName>::SetToNull(MailboxName* b) {
  b->data = nullptr;
}

// static
size_t ArrayTraits<MailboxName>::GetSize(const MailboxName& b) {
  return GL_MAILBOX_SIZE_CHROMIUM;
}

// static
int8_t* ArrayTraits<MailboxName>::GetData(MailboxName& b) {
  return b.data;
}

// static
const int8_t* ArrayTraits<MailboxName>::GetData(const MailboxName& b) {
  return b.data;
}

// static
int8_t& ArrayTraits<MailboxName>::GetAt(MailboxName& b, size_t i) {
  return b.data[i];
}

// static
const int8_t& ArrayTraits<MailboxName>::GetAt(const MailboxName& b, size_t i) {
  return b.data[i];
}

// static
void ArrayTraits<MailboxName>::Resize(MailboxName& b, size_t size) {
  DCHECK(GL_MAILBOX_SIZE_CHROMIUM == size);
}

// static
MailboxName StructTraits<gpu::mojom::Mailbox, gpu::Mailbox>::name(
    const gpu::Mailbox& mailbox) {
  MailboxName mailbox_name;
  mailbox_name.data = const_cast<int8_t*>(&mailbox.name[0]);
  return mailbox_name;
}

// static
bool StructTraits<gpu::mojom::Mailbox, gpu::Mailbox>::Read(
    gpu::mojom::MailboxDataView data,
    gpu::Mailbox* out) {
  MailboxName mailbox_name;
  mailbox_name.data = &out->name[0];
  return data.ReadName(&mailbox_name);
}

}  // namespace mojo
