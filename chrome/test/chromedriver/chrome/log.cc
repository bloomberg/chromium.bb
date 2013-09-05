// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/log.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"

void Log::AddEntry(Level level, const std::string& message) {
  AddEntryTimestamped(base::Time::Now(), level, message);
}

namespace {

IsVLogOnFunc g_is_vlog_on_func = NULL;

void TruncateString(std::string* data) {
  const size_t kMaxLength = 200;
  if (data->length() > kMaxLength) {
    data->resize(kMaxLength);
    data->replace(kMaxLength - 3, 3, "...");
  }
}

base::Value* TruncateContainedStrings(base::Value* value) {
  base::ListValue* list = NULL;
  base::DictionaryValue* dict = NULL;
  if (value->GetAsDictionary(&dict)) {
    for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd();
         it.Advance()) {
      std::string data;
      if (it.value().GetAsString(&data)) {
        TruncateString(&data);
        dict->SetWithoutPathExpansion(it.key(), new base::StringValue(data));
      } else {
        base::Value* child = NULL;
        dict->GetWithoutPathExpansion(it.key(), &child);
        TruncateContainedStrings(child);
      }
    }
  } else if (value->GetAsList(&list)) {
    for (size_t i = 0; i < list->GetSize(); ++i) {
      base::Value* child = NULL;
      if (!list->Get(i, &child))
        continue;
      std::string data;
      if (child->GetAsString(&data)) {
        TruncateString(&data);
        list->Set(i, new base::StringValue(data));
      } else {
        TruncateContainedStrings(child);
      }
    }
  }
  return value;
}

}  // namespace

void InitLogging(IsVLogOnFunc is_vlog_on_func) {
  g_is_vlog_on_func = is_vlog_on_func;
}

bool IsVLogOn(int vlog_level) {
  if (!g_is_vlog_on_func)
    return false;
  return g_is_vlog_on_func(vlog_level);
}

std::string PrettyPrintValue(const base::Value& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      &value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
#if defined(OS_WIN)
  RemoveChars(json, "\r", &json);
#endif
  // Remove the trailing newline.
  if (json.length())
    json.resize(json.length() - 1);
  return json;
}

std::string FormatValueForDisplay(const base::Value& value) {
  scoped_ptr<base::Value> truncated(TruncateContainedStrings(value.DeepCopy()));
  return PrettyPrintValue(*truncated);
}

std::string FormatJsonForDisplay(const std::string& json) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(json));
  if (!value) {
    std::string truncated = json;
    TruncateString(&truncated);
    return truncated;
  }
  return PrettyPrintValue(*TruncateContainedStrings(value.get()));
}
