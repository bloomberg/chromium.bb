// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/error.h"

namespace openscreen {

Error::Error() = default;

Error::Error(const Error& error) = default;

Error::Error(Error&& error) = default;

Error::Error(Code code) : code_(code) {}

Error::Error(Code code, const std::string& message)
    : code_(code), message_(message) {}

Error::Error(Code code, std::string&& message)
    : code_(code), message_(std::move(message)) {}

Error& Error::operator=(const Error& other) = default;

Error& Error::operator=(Error&& other) = default;

bool Error::operator==(const Error& other) const {
  return code_ == other.code_ && message_ == other.message_;
}

// static
std::string Error::CodeToString(Error::Code code) {
  switch (code) {
    case Error::Code::kNone:
      return "None";
    case Error::Code::kCborParsing:
      return "CborParsingError";
    default:
      return "Unknown";
  }
}

std::ostream& operator<<(std::ostream& out, const Error& error) {
  out << Error::CodeToString(error.code()) << ": " << error.message();
  return out;
}

}  // namespace openscreen
