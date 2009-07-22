// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_utils.h"

#include "base/json_writer.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/values.h"

namespace IPC {

const int kMaxRecursionDepth = 100;

// Value serialization

static bool ReadValue(const Message* m, void** iter, Value** value,
                      int recursion);

static void WriteValue(Message* m, const Value* value, int recursion) {
  if (recursion > kMaxRecursionDepth) {
    LOG(WARNING) << "Max recursion depth hit in WriteValue.";
    return;
  }

  m->WriteInt(value->GetType());

  switch (value->GetType()) {
  case Value::TYPE_NULL:
    break;
  case Value::TYPE_BOOLEAN: {
      bool val;
      value->GetAsBoolean(&val);
      WriteParam(m, val);
      break;
    }
  case Value::TYPE_INTEGER: {
      int val;
      value->GetAsInteger(&val);
      WriteParam(m, val);
      break;
    }
  case Value::TYPE_REAL: {
      double val;
      value->GetAsReal(&val);
      WriteParam(m, val);
      break;
    }
  case Value::TYPE_STRING: {
      std::string val;
      value->GetAsString(&val);
      WriteParam(m, val);
      break;
    }
  case Value::TYPE_BINARY: {
      NOTREACHED() << "Don't send BinaryValues over IPC.";
    }
  case Value::TYPE_DICTIONARY: {
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);

      WriteParam(m, static_cast<int>(dict->GetSize()));

      for (DictionaryValue::key_iterator it = dict->begin_keys();
           it != dict->end_keys(); ++it) {
        Value* subval;
        if (dict->Get(*it, &subval)) {
          WriteParam(m, *it);
          WriteValue(m, subval, recursion + 1);
        } else {
          NOTREACHED() << "DictionaryValue iterators are filthy liars.";
        }
      }
      break;
    }
  case Value::TYPE_LIST: {
      const ListValue* list = static_cast<const ListValue*>(value);
      WriteParam(m, static_cast<int>(list->GetSize()));
      for (size_t i = 0; i < list->GetSize(); ++i) {
        Value* subval;
        if (list->Get(i, &subval)) {
          WriteValue(m, subval, recursion + 1);
        } else {
          NOTREACHED() << "ListValue::GetSize is a filthy liar.";
        }
      }
      break;
    }
  }
}

// Helper for ReadValue that reads a DictionaryValue into a pre-allocated
// object.
static bool ReadDictionaryValue(const Message* m, void** iter,
                                DictionaryValue* value, int recursion) {
  int size;
  if (!ReadParam(m, iter, &size))
    return false;

  for (int i = 0; i < size; ++i) {
    std::wstring key;
    Value* subval;
    if (!ReadParam(m, iter, &key) ||
        !ReadValue(m, iter, &subval, recursion + 1))
      return false;
    value->Set(key, subval);
  }

  return true;
}

// Helper for ReadValue that reads a ReadListValue into a pre-allocated
// object.
static bool ReadListValue(const Message* m, void** iter,
                          ListValue* value, int recursion) {
  int size;
  if (!ReadParam(m, iter, &size))
    return false;

  for (int i = 0; i < size; ++i) {
    Value* subval;
    if (!ReadValue(m, iter, &subval, recursion + 1))
      return false;
    value->Set(i, subval);
  }

  return true;
}

static bool ReadValue(const Message* m, void** iter, Value** value,
                      int recursion) {
  if (recursion > kMaxRecursionDepth) {
    LOG(WARNING) << "Max recursion depth hit in ReadValue.";
    return false;
  }

  int type;
  if (!ReadParam(m, iter, &type))
    return false;

  switch (type) {
  case Value::TYPE_NULL:
    *value = Value::CreateNullValue();
    break;
  case Value::TYPE_BOOLEAN: {
      bool val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = Value::CreateBooleanValue(val);
      break;
    }
  case Value::TYPE_INTEGER: {
      int val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = Value::CreateIntegerValue(val);
      break;
    }
  case Value::TYPE_REAL: {
      double val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = Value::CreateRealValue(val);
      break;
    }
  case Value::TYPE_STRING: {
      std::string val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = Value::CreateStringValue(val);
      break;
    }
  case Value::TYPE_BINARY: {
      NOTREACHED() << "Don't send BinaryValues over IPC.";
      break;
    }
  case Value::TYPE_DICTIONARY: {
      scoped_ptr<DictionaryValue> val(new DictionaryValue());
      if (!ReadDictionaryValue(m, iter, val.get(), recursion))
        return false;
      *value = val.release();
      break;
    }
  case Value::TYPE_LIST: {
      scoped_ptr<ListValue> val(new ListValue());
      if (!ReadListValue(m, iter, val.get(), recursion))
        return false;
      *value = val.release();
      break;
    }
  default:
    return false;
  }

  return true;
}

void ParamTraits<DictionaryValue>::Write(Message* m, const param_type& p) {
  WriteValue(m, &p, 0);
}

bool ParamTraits<DictionaryValue>::Read(
    const Message* m, void** iter, param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type) || type != Value::TYPE_DICTIONARY)
    return false;

  return ReadDictionaryValue(m, iter, r, 0);
}

void ParamTraits<DictionaryValue>::Log(const param_type& p, std::wstring* l) {
  std::string json;
  JSONWriter::Write(&p, false, &json);
  l->append(UTF8ToWide(json));
}

void ParamTraits<ListValue>::Write(Message* m, const param_type& p) {
  WriteValue(m, &p, 0);
}

bool ParamTraits<ListValue>::Read(
    const Message* m, void** iter, param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type) || type != Value::TYPE_LIST)
    return false;

  return ReadListValue(m, iter, r, 0);
}

void ParamTraits<ListValue>::Log(const param_type& p, std::wstring* l) {
  std::string json;
  JSONWriter::Write(&p, false, &json);
  l->append(UTF8ToWide(json));
}
}  // namespace IPC
