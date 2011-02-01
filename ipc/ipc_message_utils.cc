// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_utils.h"

#include "base/file_path.h"
#include "base/json/json_writer.h"
#include "base/nullable_string16.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#if defined(OS_POSIX)
#include "ipc/file_descriptor_set_posix.h"
#endif
#include "ipc/ipc_channel_handle.h"

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
    case Value::TYPE_DOUBLE: {
      double val;
      value->GetAsDouble(&val);
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
      const BinaryValue* binary = static_cast<const BinaryValue*>(value);
      m->WriteData(binary->GetBuffer(), static_cast<int>(binary->GetSize()));
      break;
    }
    case Value::TYPE_DICTIONARY: {
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);

      WriteParam(m, static_cast<int>(dict->size()));

      for (DictionaryValue::key_iterator it = dict->begin_keys();
           it != dict->end_keys(); ++it) {
        Value* subval;
        if (dict->GetWithoutPathExpansion(*it, &subval)) {
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
    std::string key;
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
    case Value::TYPE_DOUBLE: {
      double val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = Value::CreateDoubleValue(val);
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
      const char* data;
      int length;
      if (!m->ReadData(iter, &data, &length))
        return false;
      *value = BinaryValue::CreateWithCopiedBuffer(data, length);
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

void ParamTraits<int>::Log(const param_type& p, std::string* l) {
  l->append(base::IntToString(p));
}

void ParamTraits<unsigned int>::Log(const param_type& p, std::string* l) {
  l->append(base::UintToString(p));
}

void ParamTraits<long>::Log(const param_type& p, std::string* l) {
  l->append(base::Int64ToString(static_cast<int64>(p)));
}

void ParamTraits<unsigned long>::Log(const param_type& p, std::string* l) {
  l->append(base::Uint64ToString(static_cast<uint64>(p)));
}

void ParamTraits<long long>::Log(const param_type& p, std::string* l) {
  l->append(base::Int64ToString(static_cast<int64>(p)));
}

void ParamTraits<unsigned long long>::Log(const param_type& p, std::string* l) {
  l->append(base::Uint64ToString(p));
}

void ParamTraits<unsigned short>::Write(Message* m, const param_type& p) {
  m->WriteBytes(&p, sizeof(param_type));
}

bool ParamTraits<unsigned short>::Read(const Message* m, void** iter,
                                       param_type* r) {
  const char* data;
  if (!m->ReadBytes(iter, &data, sizeof(param_type)))
    return false;
  memcpy(r, data, sizeof(param_type));
  return true;
}

void ParamTraits<unsigned short>::Log(const param_type& p, std::string* l) {
  l->append(base::UintToString(p));
}

void ParamTraits<base::Time>::Write(Message* m, const param_type& p) {
  ParamTraits<int64>::Write(m, p.ToInternalValue());
}

bool ParamTraits<base::Time>::Read(const Message* m, void** iter,
                                   param_type* r) {
  int64 value;
  if (!ParamTraits<int64>::Read(m, iter, &value))
    return false;
  *r = base::Time::FromInternalValue(value);
  return true;
}

void ParamTraits<base::Time>::Log(const param_type& p, std::string* l) {
  ParamTraits<int64>::Log(p.ToInternalValue(), l);
}

void ParamTraits<base::TimeDelta> ::Write(Message* m, const param_type& p) {
  ParamTraits<int64> ::Write(m, p.InMicroseconds());
}

bool ParamTraits<base::TimeDelta> ::Read(const Message* m,
                                         void** iter,
                                         param_type* r) {
  int64 value;
  bool ret = ParamTraits<int64> ::Read(m, iter, &value);
  if (ret)
    *r = base::TimeDelta::FromMicroseconds(value);

  return ret;
}

void ParamTraits<base::TimeDelta> ::Log(const param_type& p, std::string* l) {
  ParamTraits<int64> ::Log(p.InMicroseconds(), l);
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

void ParamTraits<DictionaryValue>::Log(const param_type& p, std::string* l) {
  std::string json;
  base::JSONWriter::Write(&p, false, &json);
  l->append(json);
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

void ParamTraits<ListValue>::Log(const param_type& p, std::string* l) {
  std::string json;
  base::JSONWriter::Write(&p, false, &json);
  l->append(json);
}

void ParamTraits<std::wstring>::Log(const param_type& p, std::string* l) {
  l->append(WideToUTF8(p));
}

void ParamTraits<NullableString16>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.string());
  WriteParam(m, p.is_null());
}

bool ParamTraits<NullableString16>::Read(const Message* m, void** iter,
                                         param_type* r) {
  string16 string;
  if (!ReadParam(m, iter, &string))
    return false;
  bool is_null;
  if (!ReadParam(m, iter, &is_null))
    return false;
  *r = NullableString16(string, is_null);
  return true;
}

void ParamTraits<NullableString16>::Log(const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.string(), l);
  l->append(", ");
  LogParam(p.is_null(), l);
  l->append(")");
}

#if !defined(WCHAR_T_IS_UTF16)
void ParamTraits<string16>::Log(const param_type& p, std::string* l) {
  l->append(UTF16ToUTF8(p));
}
#endif


void ParamTraits<FilePath>::Write(Message* m, const param_type& p) {
  ParamTraits<FilePath::StringType>::Write(m, p.value());
}

bool ParamTraits<FilePath>::Read(const Message* m, void** iter, param_type* r) {
  FilePath::StringType value;
  if (!ParamTraits<FilePath::StringType>::Read(m, iter, &value))
    return false;
  *r = FilePath(value);
  return true;
}

void ParamTraits<FilePath>::Log(const param_type& p, std::string* l) {
  ParamTraits<FilePath::StringType>::Log(p.value(), l);
}

#if defined(OS_POSIX)
void ParamTraits<base::FileDescriptor>::Write(Message* m, const param_type& p) {
  const bool valid = p.fd >= 0;
  WriteParam(m, valid);

  if (valid) {
    if (!m->WriteFileDescriptor(p))
      NOTREACHED();
  }
}

bool ParamTraits<base::FileDescriptor>::Read(const Message* m, void** iter,
                                             param_type* r) {
  bool valid;
  if (!ReadParam(m, iter, &valid))
    return false;

  if (!valid) {
    r->fd = -1;
    r->auto_close = false;
    return true;
  }

  return m->ReadFileDescriptor(iter, r);
}

void ParamTraits<base::FileDescriptor>::Log(const param_type& p,
                                            std::string* l) {
  if (p.auto_close) {
    l->append(StringPrintf("FD(%d auto-close)", p.fd));
  } else {
    l->append(StringPrintf("FD(%d)", p.fd));
  }
}
#endif  // defined(OS_POSIX)

void ParamTraits<IPC::ChannelHandle>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.name);
#if defined(OS_POSIX)
  WriteParam(m, p.socket);
#endif
}

bool ParamTraits<IPC::ChannelHandle>::Read(const Message* m, void** iter,
                                           param_type* r) {
  return ReadParam(m, iter, &r->name)
#if defined(OS_POSIX)
      && ReadParam(m, iter, &r->socket)
#endif
      ;
}

void ParamTraits<IPC::ChannelHandle>::Log(const param_type& p,
                                          std::string* l) {
  l->append(StringPrintf("ChannelHandle(%s", p.name.c_str()));
#if defined(OS_POSIX)
  ParamTraits<base::FileDescriptor>::Log(p.socket, l);
#endif
  l->append(")");
}

LogData::LogData() {
}

LogData::~LogData() {
}

void ParamTraits<LogData>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.channel);
  WriteParam(m, p.routing_id);
  WriteParam(m, static_cast<int>(p.type));
  WriteParam(m, p.flags);
  WriteParam(m, p.sent);
  WriteParam(m, p.receive);
  WriteParam(m, p.dispatch);
  WriteParam(m, p.params);
}

bool ParamTraits<LogData>::Read(const Message* m, void** iter, param_type* r) {
  int type;
  bool result =
      ReadParam(m, iter, &r->channel) &&
      ReadParam(m, iter, &r->routing_id) &&
      ReadParam(m, iter, &type) &&
      ReadParam(m, iter, &r->flags) &&
      ReadParam(m, iter, &r->sent) &&
      ReadParam(m, iter, &r->receive) &&
      ReadParam(m, iter, &r->dispatch) &&
      ReadParam(m, iter, &r->params);
  r->type = static_cast<uint16>(type);
  return result;
}

}  // namespace IPC
