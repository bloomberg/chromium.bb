// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/remoting/chromoting_host_info.h"
#include "ipc/ipc_channel_handle.h"

#define IPC_MESSAGE_IMPL
#include "chrome/common/service_messages.h"

namespace IPC {

void ParamTraits<remoting::ChromotingHostInfo> ::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.host_id);
  WriteParam(m, p.hostname);
  WriteParam(m, p.public_key);
  WriteParam(m, p.enabled);
  WriteParam(m, p.login);
}

bool ParamTraits<remoting::ChromotingHostInfo> ::Read(
    const Message* m, void** iter, param_type* p) {
  bool ret = ReadParam(m, iter, &p->host_id);
  ret = ret && ReadParam(m, iter, &p->hostname);
  ret = ret && ReadParam(m, iter, &p->public_key);
  ret = ret && ReadParam(m, iter, &p->enabled);
  ret = ret && ReadParam(m, iter, &p->login);
  return ret;
}

void ParamTraits<remoting::ChromotingHostInfo> ::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.host_id, l);
  l->append(", ");
  LogParam(p.hostname, l);
  l->append(", ");
  LogParam(p.public_key, l);
  l->append(", ");
  LogParam(p.enabled, l);
  l->append(", ");
  LogParam(p.login, l);
  l->append(")");
}

}  // namespace IPC
