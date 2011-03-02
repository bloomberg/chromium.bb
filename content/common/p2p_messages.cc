// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define IPC_MESSAGE_IMPL
#include "content/common/p2p_messages.h"

namespace IPC {

void ParamTraits<P2PSocketAddress>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.address);
  WriteParam(m, p.port);
}

bool ParamTraits<P2PSocketAddress>::Read(const Message* m,
                                         void** iter,
                                         param_type* p) {
  return
      ReadParam(m, iter, &p->address) &&
      ReadParam(m, iter, &p->port);
}

void ParamTraits<P2PSocketAddress>::Log(const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.address, l);
  l->append(", ");
  LogParam(p.port, l);
  l->append(")");
}

}  // namespace IPC
