// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/common_param_traits.h"

#define IPC_MESSAGE_IMPL
#include "chrome/common/pepper_file_messages.h"

namespace IPC {

void ParamTraits<webkit::ppapi::DirEntry>::Write(Message* m,
                                                 const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.is_dir);
}

bool ParamTraits<webkit::ppapi::DirEntry>::Read(const Message* m,
                                                void** iter,
                                                param_type* p) {
  return ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->is_dir);
}

void ParamTraits<webkit::ppapi::DirEntry>::Log(const param_type& p,
                                               std::string* l) {
  l->append("(");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.is_dir, l);
  l->append(")");
}

}  // namespace IPC
