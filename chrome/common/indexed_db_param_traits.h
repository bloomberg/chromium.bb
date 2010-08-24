// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INDEXED_DB_PARAM_TRAITS_H_
#define CHROME_COMMON_INDEXED_DB_PARAM_TRAITS_H_
#pragma once

#include "chrome/common/indexed_db_key.h"
#include "chrome/common/serialized_script_value.h"
#include "ipc/ipc_param_traits.h"

namespace IPC {

// These datatypes are used by utility_messages.h and render_messages.h.
// Unfortunately we can't move it to common: MSVC linker complains about
// WebKit datatypes that are not linked on npchrome_frame (even though it's
// never actually used by that target).

template <>
struct ParamTraits<SerializedScriptValue> {
  typedef SerializedScriptValue param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.is_null());
    WriteParam(m, p.is_invalid());
    WriteParam(m, p.data());
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
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
  static void Log(const param_type& p, std::string* l) {
    l->append("<SerializedScriptValue>(");
    LogParam(p.is_null(), l);
    l->append(", ");
    LogParam(p.is_invalid(), l);
    l->append(", ");
    LogParam(p.data(), l);
    l->append(")");
  }
};

template <>
struct ParamTraits<IndexedDBKey> {
  typedef IndexedDBKey param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, int(p.type()));
    // TODO(jorlow): Technically, we only need to pack the type being used.
    WriteParam(m, p.string());
    WriteParam(m, p.number());
  }
  static bool Read(const Message* m, void** iter, param_type* r) {
    int type;
    string16 string;
    int32 number;
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
  static void Log(const param_type& p, std::string* l) {
    l->append("<IndexedDBKey>(");
    LogParam(int(p.type()), l);
    l->append(", ");
    LogParam(p.string(), l);
    l->append(", ");
    LogParam(p.number(), l);
    l->append(")");
  }
};

} // namespace IPC

#endif  // CHROME_COMMON_INDEXED_DB_PARAM_TRAITS_H_
