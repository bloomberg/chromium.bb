// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/common_param_traits.h"
#include "ppapi/c/private/ppb_flash.h"

#define IPC_MESSAGE_IMPL
#include "chrome/common/pepper_messages.h"

namespace IPC {

void ParamTraits<PP_Flash_NetAddress>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.size);
  m->WriteBytes(p.data, static_cast<int>(p.size));
}

bool ParamTraits<PP_Flash_NetAddress>::Read(const Message* m,
                                            void** iter,
                                            param_type* p) {
  uint16 size;
  if (!ReadParam(m, iter, &size))
    return false;
  if (size > sizeof(p->data))
    return false;
  p->size = size;

  const char* data;
  if (!m->ReadBytes(iter, &data, size))
    return false;
  memcpy(p->data, data, size);
  return true;
}

void ParamTraits<PP_Flash_NetAddress>::Log(const param_type& p,
                                           std::string* l) {
  l->append("<PP_Flash_NetAddress (");
  LogParam(p.size, l);
  l->append(" bytes)>");
}

}  // namespace IPC
