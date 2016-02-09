// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/gpu_command_buffer_traits.h"

#include <stddef.h>
#include <stdint.h>

#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/common/value_state.h"

// Generate param traits size methods.
#include "ipc/param_traits_size_macros.h"
namespace IPC {
#include "gpu/ipc/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "gpu/ipc/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "gpu/ipc/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "gpu/ipc/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

namespace IPC {

void ParamTraits<gpu::CommandBuffer::State>::GetSize(base::PickleSizer* s,
                                                     const param_type& p) {
  GetParamSize(s, p.get_offset);
  GetParamSize(s, p.token);
  GetParamSize(s, p.error);
  GetParamSize(s, p.generation);
}

void ParamTraits<gpu::CommandBuffer::State>::Write(base::Pickle* m,
                                                   const param_type& p) {
  WriteParam(m, p.get_offset);
  WriteParam(m, p.token);
  WriteParam(m, p.error);
  WriteParam(m, p.generation);
}

bool ParamTraits<gpu::CommandBuffer::State>::Read(const base::Pickle* m,
                                                  base::PickleIterator* iter,
                                                  param_type* p) {
  if (ReadParam(m, iter, &p->get_offset) &&
      ReadParam(m, iter, &p->token) &&
      ReadParam(m, iter, &p->error) &&
      ReadParam(m, iter, &p->generation)) {
    return true;
  } else {
    return false;
  }
}

void ParamTraits<gpu::CommandBuffer::State> ::Log(const param_type& p,
                                                  std::string* l) {
  l->append("<CommandBuffer::State>");
}

void ParamTraits<gpu::SyncToken>::GetSize(base::PickleSizer* s,
                                          const param_type& p) {
  DCHECK(!p.HasData() || p.verified_flush());
  GetParamSize(s, p.verified_flush());
  GetParamSize(s, p.namespace_id());
  GetParamSize(s, p.command_buffer_id());
  GetParamSize(s, p.release_count());
}

void ParamTraits<gpu::SyncToken>::Write(base::Pickle* m, const param_type& p) {
  DCHECK(!p.HasData() || p.verified_flush());

  WriteParam(m, p.verified_flush());
  WriteParam(m, p.namespace_id());
  WriteParam(m, p.command_buffer_id());
  WriteParam(m, p.release_count());
}

bool ParamTraits<gpu::SyncToken>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  bool verified_flush = false;
  gpu::CommandBufferNamespace namespace_id =
      gpu::CommandBufferNamespace::INVALID;
  uint64_t command_buffer_id = 0;
  uint64_t release_count = 0;

  if (!ReadParam(m, iter, &verified_flush) ||
      !ReadParam(m, iter, &namespace_id) ||
      !ReadParam(m, iter, &command_buffer_id) ||
      !ReadParam(m, iter, &release_count)) {
    return false;
  }

  p->Set(namespace_id, 0, command_buffer_id, release_count);
  if (p->HasData()) {
    if (!verified_flush)
      return false;
    p->SetVerifyFlush();
  }

  return true;
}

void ParamTraits<gpu::SyncToken>::Log(const param_type& p, std::string* l) {
  *l +=
      base::StringPrintf("[%d:%llX] %llu", static_cast<int>(p.namespace_id()),
                         static_cast<unsigned long long>(p.command_buffer_id()),
                         static_cast<unsigned long long>(p.release_count()));
}

void ParamTraits<gpu::Mailbox>::GetSize(base::PickleSizer* s,
                                        const param_type& p) {
  s->AddBytes(sizeof(p.name));
}

void ParamTraits<gpu::Mailbox>::Write(base::Pickle* m, const param_type& p) {
  m->WriteBytes(p.name, sizeof(p.name));
}

bool ParamTraits<gpu::Mailbox>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     param_type* p) {
  const char* bytes = NULL;
  if (!iter->ReadBytes(&bytes, sizeof(p->name)))
    return false;
  DCHECK(bytes);
  memcpy(p->name, bytes, sizeof(p->name));
  return true;
}

void ParamTraits<gpu::Mailbox>::Log(const param_type& p, std::string* l) {
  for (size_t i = 0; i < sizeof(p.name); ++i)
    *l += base::StringPrintf("%02x", p.name[i]);
}

void ParamTraits<gpu::MailboxHolder>::GetSize(base::PickleSizer* s,
                                              const param_type& p) {
  GetParamSize(s, p.mailbox);
  GetParamSize(s, p.sync_token);
  GetParamSize(s, p.texture_target);
}

void ParamTraits<gpu::MailboxHolder>::Write(base::Pickle* m,
                                            const param_type& p) {
  WriteParam(m, p.mailbox);
  WriteParam(m, p.sync_token);
  WriteParam(m, p.texture_target);
}

bool ParamTraits<gpu::MailboxHolder>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           param_type* p) {
  if (!ReadParam(m, iter, &p->mailbox) || !ReadParam(m, iter, &p->sync_token) ||
      !ReadParam(m, iter, &p->texture_target))
    return false;
  return true;
}

void ParamTraits<gpu::MailboxHolder>::Log(const param_type& p, std::string* l) {
  LogParam(p.mailbox, l);
  LogParam(p.sync_token, l);
  *l += base::StringPrintf(":%04x@", p.texture_target);
}

void ParamTraits<gpu::ValueState>::GetSize(base::PickleSizer* s,
                                           const param_type& p) {
  s->AddData(sizeof(gpu::ValueState));
}

void ParamTraits<gpu::ValueState>::Write(base::Pickle* m, const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(&p),
               sizeof(gpu::ValueState));
}

bool ParamTraits<gpu::ValueState>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        param_type* p) {
  int length;
  const char* data = NULL;
  if (!iter->ReadData(&data, &length) || length != sizeof(gpu::ValueState))
    return false;
  DCHECK(data);
  memcpy(p, data, sizeof(gpu::ValueState));
  return true;
}

void ParamTraits<gpu::ValueState>::Log(const param_type& p, std::string* l) {
  l->append("<ValueState (");
  for (int value : p.int_value)
    *l += base::StringPrintf("%i ", value);
  l->append(" int values ");
  for (float value : p.float_value)
    *l += base::StringPrintf("%f ", value);
  l->append(" float values)>");
}

}  // namespace IPC
