// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_value_serializer.h"

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"

const char* JSONFileValueSerializer::kAccessDenied = "Access denied.";
const char* JSONFileValueSerializer::kCannotReadFile = "Can't read file.";
const char* JSONFileValueSerializer::kFileLocked = "File locked.";
const char* JSONFileValueSerializer::kNoSuchFile = "File doesn't exist.";

JSONStringValueSerializer::~JSONStringValueSerializer() {}

bool JSONStringValueSerializer::Serialize(const Value& root) {
  return SerializeInternal(root, false);
}

bool JSONStringValueSerializer::SerializeAndOmitBinaryValues(
    const Value& root) {
  return SerializeInternal(root, true);
}

bool JSONStringValueSerializer::SerializeInternal(const Value& root,
                                                  bool omit_binary_values) {
  if (!json_string_ || initialized_with_const_string_)
    return false;

  base::JSONWriter::WriteWithOptions(
      &root,
      pretty_print_,
      omit_binary_values ? base::JSONWriter::OPTIONS_OMIT_BINARY_VALUES : 0,
      json_string_);
  return true;
}

Value* JSONStringValueSerializer::Deserialize(int* error_code,
                                              std::string* error_str) {
  if (!json_string_)
    return NULL;

  return base::JSONReader::ReadAndReturnError(*json_string_,
                                              allow_trailing_comma_,
                                              error_code,
                                              error_str);
}

/******* File Serializer *******/

bool JSONFileValueSerializer::Serialize(const Value& root) {
  return SerializeInternal(root, false);
}

bool JSONFileValueSerializer::SerializeAndOmitBinaryValues(const Value& root) {
  return SerializeInternal(root, true);
}

bool JSONFileValueSerializer::SerializeInternal(const Value& root,
                                                bool omit_binary_values) {
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.set_pretty_print(true);
  bool result = omit_binary_values ?
      serializer.SerializeAndOmitBinaryValues(root) :
      serializer.Serialize(root);
  if (!result)
    return false;

  int data_size = static_cast<int>(json_string.size());
  if (file_util::WriteFile(json_file_path_,
                           json_string.data(),
                           data_size) != data_size)
    return false;

  return true;
}

int JSONFileValueSerializer::ReadFileToString(std::string* json_string) {
  DCHECK(json_string);
  if (!file_util::ReadFileToString(json_file_path_, json_string)) {
#if defined(OS_WIN)
    int error = ::GetLastError();
    if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) {
      return JSON_FILE_LOCKED;
    } else if (error == ERROR_ACCESS_DENIED) {
      return JSON_ACCESS_DENIED;
    }
#endif
    if (!file_util::PathExists(json_file_path_))
      return JSON_NO_SUCH_FILE;
    else
      return JSON_CANNOT_READ_FILE;
  }
  return JSON_NO_ERROR;
}

const char* JSONFileValueSerializer::GetErrorMessageForCode(int error_code) {
  switch (error_code) {
    case JSON_NO_ERROR:
      return "";
    case JSON_ACCESS_DENIED:
      return kAccessDenied;
    case JSON_CANNOT_READ_FILE:
      return kCannotReadFile;
    case JSON_FILE_LOCKED:
      return kFileLocked;
    case JSON_NO_SUCH_FILE:
      return kNoSuchFile;
    default:
      NOTREACHED();
      return "";
  }
}

Value* JSONFileValueSerializer::Deserialize(int* error_code,
                                            std::string* error_str) {
  std::string json_string;
  int error = ReadFileToString(&json_string);
  if (error != JSON_NO_ERROR) {
    if (error_code)
      *error_code = error;
    if (error_str)
      *error_str = GetErrorMessageForCode(error);
    return NULL;
  }

  JSONStringValueSerializer serializer(json_string);
  serializer.set_allow_trailing_comma(allow_trailing_comma_);
  return serializer.Deserialize(error_code, error_str);
}
