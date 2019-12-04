// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_UTIL_H_
#define CAST_STREAMING_MESSAGE_UTIL_H_

#include "json/value.h"
#include "platform/base/error.h"

namespace cast {
namespace streaming {

inline openscreen::Error CreateParseError(const std::string& type) {
  return openscreen::Error(openscreen::Error::Code::kJsonParseError,
                           "Failed to parse " + type);
}

inline openscreen::Error CreateParameterError(const std::string& type) {
  return openscreen::Error(openscreen::Error::Code::kParameterInvalid,
                           "Invalid parameter: " + type);
}

inline openscreen::ErrorOr<bool> ParseBool(const Json::Value& parent,
                                           const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isBool()) {
    return CreateParseError("bool field " + field);
  }
  return value.asBool();
}

inline openscreen::ErrorOr<int> ParseInt(const Json::Value& parent,
                                         const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isInt()) {
    return CreateParseError("integer field: " + field);
  }
  return value.asInt();
}

inline openscreen::ErrorOr<uint32_t> ParseUint(const Json::Value& parent,
                                               const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isUInt()) {
    return CreateParseError("unsigned integer field: " + field);
  }
  return value.asUInt();
}

inline openscreen::ErrorOr<std::string> ParseString(const Json::Value& parent,
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
T ValueOrDefault(const openscreen::ErrorOr<T>& value, T fallback = T{}) {
  if (value.is_value()) {
    return value.value();
  }
  return fallback;
}

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_MESSAGE_UTIL_H_
