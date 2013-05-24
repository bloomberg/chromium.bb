// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_param_traits.h"

#include <string>
#include <vector>
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "ipc/ipc_message_utils.h"

using content::IndexedDBKey;
using content::IndexedDBKeyPath;
using content::IndexedDBKeyRange;

using WebKit::WebIDBKey;
using WebKit::WebIDBKeyPath;

namespace IPC {

void ParamTraits<IndexedDBKey>::Write(Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p.type()));
  switch (p.type()) {
    case WebIDBKey::ArrayType:
      WriteParam(m, p.array());
      return;
    case WebIDBKey::StringType:
      WriteParam(m, p.string());
      return;
    case WebIDBKey::DateType:
      WriteParam(m, p.date());
      return;
    case WebIDBKey::NumberType:
      WriteParam(m, p.number());
      return;
    case WebIDBKey::InvalidType:
    case WebIDBKey::NullType:
      return;
    case WebIDBKey::MinType:
      NOTREACHED();
      return;
  }
}

bool ParamTraits<IndexedDBKey>::Read(const Message* m,
                                     PickleIterator* iter,
                                     param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type))
    return false;
  WebIDBKey::Type web_type = static_cast<WebIDBKey::Type>(type);

  switch (web_type) {
    case WebIDBKey::ArrayType: {
      std::vector<IndexedDBKey> array;
      if (!ReadParam(m, iter, &array))
        return false;
      *r = IndexedDBKey(array);
      return true;
    }
    case WebIDBKey::StringType: {
      string16 string;
      if (!ReadParam(m, iter, &string))
        return false;
      *r = IndexedDBKey(string);
      return true;
    }
    case WebIDBKey::DateType:
    case WebIDBKey::NumberType: {
      double number;
      if (!ReadParam(m, iter, &number))
        return false;
      *r = IndexedDBKey(number, web_type);
      return true;
    }
    case WebIDBKey::InvalidType:
    case WebIDBKey::NullType:
      *r = IndexedDBKey(web_type);
      return true;
    case WebIDBKey::MinType:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
  return false;
}

void ParamTraits<IndexedDBKey>::Log(const param_type& p, std::string* l) {
  l->append("<IndexedDBKey>(");
  LogParam(static_cast<int>(p.type()), l);
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

void ParamTraits<IndexedDBKeyPath>::Write(Message* m, const param_type& p) {
  WriteParam(m, static_cast<int>(p.type()));
  switch (p.type()) {
    case WebIDBKeyPath::ArrayType:
      WriteParam(m, p.array());
      return;
    case WebIDBKeyPath::StringType:
      WriteParam(m, p.string());
      return;
    case WebIDBKeyPath::NullType:
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
    case WebIDBKeyPath::ArrayType: {
      std::vector<string16> array;
      if (!ReadParam(m, iter, &array))
        return false;
      *r = IndexedDBKeyPath(array);
      return true;
    }
    case WebIDBKeyPath::StringType: {
      string16 string;
      if (!ReadParam(m, iter, &string))
        return false;
      *r = IndexedDBKeyPath(string);
      return true;
    }
    case WebIDBKeyPath::NullType:
      *r = IndexedDBKeyPath();
      return true;
  }
  NOTREACHED();
  return false;
}

void ParamTraits<IndexedDBKeyPath>::Log(const param_type& p, std::string* l) {
  l->append("<IndexedDBKeyPath>(");
  LogParam(static_cast<int>(p.type()), l);
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

void ParamTraits<IndexedDBKeyRange>::Write(Message* m, const param_type& p) {
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

  *r = IndexedDBKeyRange(lower, upper, lower_open, upper_open);
  return true;
}

void ParamTraits<IndexedDBKeyRange>::Log(const param_type& p, std::string* l) {
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
