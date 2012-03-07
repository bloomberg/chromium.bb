// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/gpu_command_buffer_traits.h"

namespace IPC {

void ParamTraits<gpu::CommandBuffer::State> ::Write(Message* m,
                                                    const param_type& p) {
  WriteParam(m, p.num_entries);
  WriteParam(m, p.get_offset);
  WriteParam(m, p.put_offset);
  WriteParam(m, p.token);
  WriteParam(m, static_cast<int32>(p.error));
  WriteParam(m, p.generation);
}

bool ParamTraits<gpu::CommandBuffer::State> ::Read(const Message* m,
                                                   PickleIterator* iter,
                                                   param_type* p) {
  int32 temp;
  if (ReadParam(m, iter, &p->num_entries) &&
      ReadParam(m, iter, &p->get_offset) &&
      ReadParam(m, iter, &p->put_offset) &&
      ReadParam(m, iter, &p->token) &&
      ReadParam(m, iter, &temp) &&
      ReadParam(m, iter, &p->generation)) {
    p->error = static_cast<gpu::error::Error>(temp);
    return true;
  } else {
    return false;
  }
}

void ParamTraits<gpu::CommandBuffer::State> ::Log(const param_type& p,
                                                  std::string* l) {
  l->append("<CommandBuffer::State>");
}

}  // namespace IPC
