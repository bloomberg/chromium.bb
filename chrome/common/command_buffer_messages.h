// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_COMMAND_BUFFER_MESSAGES_H_
#define CHROME_COMMON_COMMAND_BUFFER_MESSAGES_H_

#include "chrome/common/common_param_traits.h"
#include "gpu/command_buffer/common/command_buffer.h"

namespace IPC {
template <>
struct ParamTraits<gpu::CommandBuffer::State> {
  typedef gpu::CommandBuffer::State param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p.size);
    m->WriteInt(p.get_offset);
    m->WriteInt(p.put_offset);
    m->WriteInt(p.token);
    m->WriteInt(p.error);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    int32 temp;
    if (m->ReadInt(iter, &p->size) &&
        m->ReadInt(iter, &p->get_offset) &&
        m->ReadInt(iter, &p->put_offset) &&
        m->ReadInt(iter, &p->token) &&
        m->ReadInt(iter, &temp)) {
      p->error = static_cast<gpu::error::Error>(temp);
      return true;
    } else {
      return false;
    }
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"<CommandBuffer::State>");
  }
};
}  // namespace IPC

#define MESSAGES_INTERNAL_FILE "chrome/common/command_buffer_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_COMMAND_BUFFER_MESSAGES_H_
