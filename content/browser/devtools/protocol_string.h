// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STRING_H
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STRING_H

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/content_export.h"

namespace base {
class Value;
}

namespace content {
namespace protocol {

class Value;

using String = std::string;

class CONTENT_EXPORT StringBuilder {
 public:
  StringBuilder();
  ~StringBuilder();
  void append(const std::string&);
  void append(char);
  void append(const char*, size_t);
  std::string toString();
  void reserveCapacity(size_t);

 private:
  std::string string_;
};

class CONTENT_EXPORT StringUtil {
 public:
  static String substring(const String& s, unsigned pos, unsigned len) {
    return s.substr(pos, len);
  }
  static String fromInteger(int number) {
    return base::IntToString(number);
  }
  static String fromDouble(double number) {
    String s = base::DoubleToString(number);
    if (!s.empty() && s[0] == '.')
      s = "0" + s;
    return s;
  }
  static double toDouble(const char* s, size_t len, bool* ok) {
    double v = 0.0;
    *ok = base::StringToDouble(std::string(s, len), &v);
    return *ok ? v : 0.0;
  }
  static size_t find(const String& s, const char* needle) {
    return s.find(needle);
  }
  static size_t find(const String& s, const String& needle) {
    return s.find(needle);
  }
  static const size_t kNotFound = static_cast<size_t>(-1);
  static void builderAppend(StringBuilder& builder, const String& s) {
    builder.append(s);
  }
  static void builderAppend(StringBuilder& builder, char c) {
    builder.append(c);
  }
  static void builderAppend(StringBuilder& builder, const char* s, size_t len) {
    builder.append(s, len);
  }
  static void builderReserve(StringBuilder& builder, unsigned capacity) {
    builder.reserveCapacity(capacity);
  }
  static String builderToString(StringBuilder& builder) {
    return builder.toString();
  }
  static std::unique_ptr<protocol::Value> parseJSON(const String&);
};

std::unique_ptr<protocol::Value> toProtocolValue(
    const base::Value* value, int depth);
std::unique_ptr<base::Value> toBaseValue(protocol::Value* value, int depth);

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_STRING_H
