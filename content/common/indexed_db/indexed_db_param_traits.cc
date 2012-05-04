// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_param_traits.h"

#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "content/public/common/serialized_script_value.h"
#include "ipc/ipc_message_utils.h"

using content::IndexedDBKey;
using content::IndexedDBKeyPath;
using content::IndexedDBKeyRange;
using content::SerializedScriptValue;

namespace IPC {

void ParamTraits<SerializedScriptValue>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, p.is_null());
  WriteParam(m, p.is_invalid());
  WriteParam(m, p.data());
}

bool ParamTraits<SerializedScriptValue>::Read(const Message* m,
                                              PickleIterator* iter,
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

void ParamTraits<IndexedDBKey>::Write(Message* m,
                                      const param_type& p) {
  WriteParam(m, int(p.type()));
  switch (p.type()) {
    case WebKit::WebIDBKey::ArrayType:
      WriteParam(m, p.array());
      return;
    case WebKit::WebIDBKey::StringType:
      WriteParam(m, p.string());
      return;
    case WebKit::WebIDBKey::DateType:
      WriteParam(m, p.date());
      return;
    case WebKit::WebIDBKey::NumberType:
      WriteParam(m, p.number());
      return;
    case WebKit::WebIDBKey::InvalidType:
    case WebKit::WebIDBKey::NullType:
      return;
  }
  NOTREACHED();
}

bool ParamTraits<IndexedDBKey>::Read(const Message* m,
                                     PickleIterator* iter,
                                     param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;

  switch (type) {
    case WebKit::WebIDBKey::ArrayType: {
      std::vector<IndexedDBKey> array;
      if (!ReadParam(m, iter, &array))
        return false;
      r->SetArray(array);
      return true;
    }
    case WebKit::WebIDBKey::StringType: {
      string16 string;
      if (!ReadParam(m, iter, &string))
        return false;
      r->SetString(string);
      return true;
    }
    case WebKit::WebIDBKey::DateType: {
      double date;
      if (!ReadParam(m, iter, &date))
        return false;
      r->SetDate(date);
      return true;
    }
    case WebKit::WebIDBKey::NumberType: {
      double number;
      if (!ReadParam(m, iter, &number))
        return false;
      r->SetNumber(number);
      return true;
    }
    case WebKit::WebIDBKey::InvalidType:
      r->SetInvalid();
      return true;
    case WebKit::WebIDBKey::NullType:
      r->SetNull();
      return true;
  }
  NOTREACHED();
  return false;
}

void ParamTraits<IndexedDBKey>::Log(const param_type& p,
                                    std::string* l) {
  l->append("<IndexedDBKey>(");
  LogParam(int(p.type()), l);
  l->append(", ");
  l->append("[");
  std::vector<IndexedDBKey>::const_iterator it = p.array().begin();
  while (it != p.array().end()) {
    Log(*it, l);
    ++it;
    if (it != p.array().end())
      l->append(", ");
  }
  l->append("], ");
  LogParam(p.string(), l);
  l->append(", ");
  LogParam(p.date(), l);
  l->append(", ");
  LogParam(p.number(), l);
  l->append(")");
}

void ParamTraits<IndexedDBKeyPath>::Write(Message* m,
                                          const param_type& p) {
  WriteParam(m, int(p.type()));
  switch (p.type()) {
    case WebKit::WebIDBKeyPath::ArrayType:
      WriteParam(m, p.array());
      return;
    case WebKit::WebIDBKeyPath::StringType:
      WriteParam(m, p.string());
      return;
    case WebKit::WebIDBKeyPath::NullType:
      return;
  }
  NOTREACHED();
}

bool ParamTraits<IndexedDBKeyPath>::Read(const Message* m,
                                         PickleIterator* iter,
                                         param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;

  switch (type) {
    case WebKit::WebIDBKeyPath::ArrayType: {
      std::vector<string16> array;
      if (!ReadParam(m, iter, &array))
        return false;
      r->SetArray(array);
      return true;
    }
    case WebKit::WebIDBKeyPath::StringType: {
      string16 string;
      if (!ReadParam(m, iter, &string))
        return false;
      r->SetString(string);
      return true;
    }
    case WebKit::WebIDBKeyPath::NullType:
      r->SetNull();
      return true;
  }
  NOTREACHED();
  return false;
}

void ParamTraits<IndexedDBKeyPath>::Log(const param_type& p,
                                        std::string* l) {
  l->append("<IndexedDBKeyPath>(");
  LogParam(int(p.type()), l);
  l->append(", ");
  LogParam(p.string(), l);
  l->append(", ");
  l->append("[");
  std::vector<string16>::const_iterator it = p.array().begin();
  while (it != p.array().end()) {
    LogParam(*it, l);
    ++it;
    if (it != p.array().end())
      l->append(", ");
  }
  l->append("])");
}

void ParamTraits<IndexedDBKeyRange>::Write(Message* m,
                                           const param_type& p) {
  WriteParam(m, p.lower());
  WriteParam(m, p.upper());
  WriteParam(m, p.lowerOpen());
  WriteParam(m, p.upperOpen());
}

bool ParamTraits<IndexedDBKeyRange>::Read(const Message* m,
                                          PickleIterator* iter,
                                          param_type* r) {
  IndexedDBKey lower;
  if (!ReadParam(m, iter, &lower))
    return false;

  IndexedDBKey upper;
  if (!ReadParam(m, iter, &upper))
    return false;

  bool lower_open;
  if (!ReadParam(m, iter, &lower_open))
    return false;

  bool upper_open;
  if (!ReadParam(m, iter, &upper_open))
    return false;

  r->Set(lower, upper, lower_open, upper_open);
  return true;
}

void ParamTraits<IndexedDBKeyRange>::Log(const param_type& p,
                                         std::string* l) {
  l->append("<IndexedDBKeyRange>(lower=");
  LogParam(p.lower(), l);
  l->append(", upper=");
  LogParam(p.upper(), l);
  l->append(", lower_open=");
  LogParam(p.lowerOpen(), l);
  l->append(", upper_open=");
  LogParam(p.upperOpen(), l);
  l->append(")");
}

}  // namespace IPC
