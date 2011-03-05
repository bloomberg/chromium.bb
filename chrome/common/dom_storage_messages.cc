// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/common_param_traits.h"

#define IPC_MESSAGE_IMPL
#include "chrome/common/dom_storage_messages.h"

DOMStorageMsg_Event_Params::DOMStorageMsg_Event_Params()
    : storage_type(DOM_STORAGE_LOCAL) {
}

DOMStorageMsg_Event_Params::~DOMStorageMsg_Event_Params() {
}

namespace IPC {

void ParamTraits<DOMStorageMsg_Event_Params>::Write(Message* m,
                                                    const param_type& p) {
  WriteParam(m, p.key);
  WriteParam(m, p.old_value);
  WriteParam(m, p.new_value);
  WriteParam(m, p.origin);
  WriteParam(m, p.url);
  WriteParam(m, p.storage_type);
}

bool ParamTraits<DOMStorageMsg_Event_Params>::Read(const Message* m,
                                                   void** iter,
                                                   param_type* p) {
  return
      ReadParam(m, iter, &p->key) &&
      ReadParam(m, iter, &p->old_value) &&
      ReadParam(m, iter, &p->new_value) &&
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->storage_type);
}

void ParamTraits<DOMStorageMsg_Event_Params>::Log(const param_type& p,
                                                  std::string* l) {
  l->append("(");
  LogParam(p.key, l);
  l->append(", ");
  LogParam(p.old_value, l);
  l->append(", ");
  LogParam(p.new_value, l);
  l->append(", ");
  LogParam(p.origin, l);
  l->append(", ");
  LogParam(p.url, l);
  l->append(", ");
  LogParam(p.storage_type, l);
  l->append(")");
}

void ParamTraits<DOMStorageType>::Write(Message* m, const param_type& p) {
  m->WriteInt(p);
}

bool ParamTraits<DOMStorageType>::Read(const Message* m,
                                       void** iter,
                                       param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<param_type>(type);
  return true;
}

void ParamTraits<DOMStorageType>::Log(const param_type& p, std::string* l) {
  std::string control;
  switch (p) {
    case DOM_STORAGE_LOCAL:
      control = "DOM_STORAGE_LOCAL";
      break;
    case DOM_STORAGE_SESSION:
      control = "DOM_STORAGE_SESSION";
      break;
    default:
      NOTIMPLEMENTED();
      control = "UNKNOWN";
      break;
  }
  LogParam(control, l);
}

void ParamTraits<WebKit::WebStorageArea::Result>::Write(Message* m,
                                                        const param_type& p) {
  m->WriteInt(p);
}

bool ParamTraits<WebKit::WebStorageArea::Result>::Read(const Message* m,
                                                       void** iter,
                                                       param_type* p) {
  int type;
  if (!m->ReadInt(iter, &type))
    return false;
  *p = static_cast<param_type>(type);
  return true;
}

void ParamTraits<WebKit::WebStorageArea::Result>::Log(const param_type& p,
                                                      std::string* l) {
  std::string control;
  switch (p) {
    case WebKit::WebStorageArea::ResultOK:
      control = "WebKit::WebStorageArea::ResultOK";
      break;
    case WebKit::WebStorageArea::ResultBlockedByQuota:
      control = "WebKit::WebStorageArea::ResultBlockedByQuota";
      break;
    case WebKit::WebStorageArea::ResultBlockedByPolicy:
      control = "WebKit::WebStorageArea::ResultBlockedByPolicy";
      break;
    default:
      NOTIMPLEMENTED();
      control = "UNKNOWN";
      break;
  }
  LogParam(control, l);
}

}  // namespace IPC
