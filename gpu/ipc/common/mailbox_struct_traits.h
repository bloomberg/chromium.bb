// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_MAILBOX_STRUCT_TRAITS_H_
#define GPU_IPC_COMMON_MAILBOX_STRUCT_TRAITS_H_

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/mailbox.mojom.h"
#include "mojo/public/cpp/bindings/array_traits.h"

namespace mojo {

// A buffer used to read bytes directly from MailboxDataView to gpu::Mailbox
// name.
struct MailboxName {
  int8_t* data;
};

// ArrayTraits needed for ReadName use with MailboxName.
template <>
struct ArrayTraits<MailboxName> {
  using Element = int8_t;
  static bool IsNull(const MailboxName& b);
  static void SetToNull(MailboxName* b);
  static size_t GetSize(const MailboxName& b);
  static int8_t* GetData(MailboxName& b);
  static const int8_t* GetData(const MailboxName& b);
  static int8_t& GetAt(MailboxName& b, size_t i);
  static const int8_t& GetAt(const MailboxName& b, size_t i);
  static void Resize(MailboxName& b, size_t size);
};

template <>
struct StructTraits<gpu::mojom::Mailbox, gpu::Mailbox> {
  static MailboxName name(const gpu::Mailbox& mailbox);
  static bool Read(gpu::mojom::MailboxDataView data, gpu::Mailbox* out);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_MAILBOX_STRUCT_TRAITS_H_
