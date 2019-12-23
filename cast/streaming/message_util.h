// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_UTIL_H_
#define CAST_STREAMING_MESSAGE_UTIL_H_

#include "json/value.h"
#include "platform/base/error.h"

namespace openscreen {
namespace cast {

inline Error CreateParseError(const std::string& type) {
  return Error(Error::Code::kJsonParseError, "Failed to parse " + type);
}

inline Error CreateParameterError(const std::string& type) {
  return Error(Error::Code::kParameterInvalid, "Invalid parameter: " + type);
}

inline ErrorOr<bool> ParseBool(const Json::Value& parent,
                               const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isBool()) {
    return CreateParseError("bool field " + field);
  }
  return value.asBool();
}

inline ErrorOr<int> ParseInt(const Json::Value& parent,
                             const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isInt()) {
    return CreateParseError("integer field: " + field);
  }
  return value.asInt();
}

inline ErrorOr<uint32_t> ParseUint(const Json::Value& parent,
                                   const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isUInt()) {
    return CreateParseError("unsigned integer field: " + field);
  }
  return value.asUInt();
}

inline ErrorOr<std::string> ParseString(const Json::Value& parent,
                                        const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isString()) {
    return CreateParseError("string field: " + field);
  }
  return value.asString();
}

// TODO(jophba): refactor to be on ErrorOr itself.
// Use this template for parsing only when there is a reasonable default
// for the type you are using, e.g. int or std::string.
template <typename T>
T ValueOrDefault(const ErrorOr<T>& value, T fallback = T{}) {
  if (value.is_value()) {
    return value.value();
  }
  return fallback;
}

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_MESSAGE_UTIL_H_
