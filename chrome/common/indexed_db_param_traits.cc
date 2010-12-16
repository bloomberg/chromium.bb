// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/indexed_db_param_traits.h"

#include "chrome/common/indexed_db_key.h"
#include "chrome/common/serialized_script_value.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

void ParamTraits<SerializedScriptValue>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, p.is_null());
  WriteParam(m, p.is_invalid());
  WriteParam(m, p.data());
}

bool ParamTraits<SerializedScriptValue>::Read(const Message* m,
                                              void** iter,
                                              param_type* r) {
  bool is_null;
  bool is_invalid;
  string16 data;
  bool ok =
      ReadParam(m, iter, &is_null) &&
      ReadParam(m, iter, &is_invalid) &&
      ReadParam(m, iter, &data);
  if (!ok)
  return false;
  r->set_is_null(is_null);
  r->set_is_invalid(is_invalid);
  r->set_data(data);
  return true;
}

void ParamTraits<SerializedScriptValue>::Log(const param_type& p,
                                             std::string* l) {
  l->append("<SerializedScriptValue>(");
  LogParam(p.is_null(), l);
  l->append(", ");
  LogParam(p.is_invalid(), l);
  l->append(", ");
  LogParam(p.data(), l);
  l->append(")");
}

void ParamTraits<IndexedDBKey>::Write(Message* m, const param_type& p) {
  WriteParam(m, int(p.type()));
  // TODO(jorlow): Technically, we only need to pack the type being used.
  WriteParam(m, p.string());
  WriteParam(m, p.number());
}

bool ParamTraits<IndexedDBKey>::Read(const Message* m,
                                     void** iter,
                                     param_type* r) {
  int type;
  string16 string;
  double number;
  bool ok =
      ReadParam(m, iter, &type) &&
      ReadParam(m, iter, &string) &&
      ReadParam(m, iter, &number);
  if (!ok)
    return false;
  switch (type) {
    case WebKit::WebIDBKey::NullType:
      r->SetNull();
      return true;
    case WebKit::WebIDBKey::StringType:
      r->Set(string);
      return true;
    case WebKit::WebIDBKey::NumberType:
      r->Set(number);
      return true;
    case WebKit::WebIDBKey::InvalidType:
      r->SetInvalid();
      return true;
  }
  NOTREACHED();
  return false;
}

void ParamTraits<IndexedDBKey>::Log(const param_type& p, std::string* l) {
  l->append("<IndexedDBKey>(");
  LogParam(int(p.type()), l);
  l->append(", ");
  LogParam(p.string(), l);
  l->append(", ");
  LogParam(p.number(), l);
  l->append(")");
}

}  // namespace IPC
