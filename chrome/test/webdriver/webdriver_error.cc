// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_error.h"

#include <sstream>

namespace webdriver {

namespace {

// Returns the string equivalent of the given |ErrorCode|.
const char* ErrorCodeToString(ErrorCode code) {
  switch (code) {
    case kSuccess:
      return "SUCCESS";
    case kNoSuchElement:
      return "NO_SUCH_ELEMENT";
    case kNoSuchFrame:
      return "NO_SUCH_FRAME";
    case kUnknownCommand:
      return "UNKNOWN_COMMAND";
    case kStaleElementReference:
      return "STALE_ELEMENT_REFERENCE";
    case kElementNotVisible:
      return "ELEMENT_NOT_VISIBLE";
    case kInvalidElementState:
      return "INVALID_ELEMENT_STATE";
    case kUnknownError:
      return "UNKNOWN_ERROR";
    case kElementNotSelectable:
      return "ELEMENT_NOT_SELECTABLE";
    case kXPathLookupError:
      return "XPATH_LOOKUP_ERROR";
    case kNoSuchWindow:
      return "NO_SUCH_WINDOW";
    case kInvalidCookieDomain:
      return "INVALID_COOKIE_DOMAIN";
    case kUnableToSetCookie:
      return "UNABLE_TO_SET_COOKIE";
    default:
      return "<unknown>";
  }
}

}  // namespace

Error::Error(ErrorCode code): code_(code) {
}

Error::Error(ErrorCode code, const std::string& details)
    : code_(code), details_(details) {
}

Error::~Error() {
}

void Error::AddDetails(const std::string& details) {
  if (details_.empty())
    details_ = details;
  else
    details_ = details + ";\n " + details_;
}

std::string Error::ToString() const {
  std::string error;
  if (code_ != kUnknownError) {
    error += ErrorCodeToString(code_);
    error += ": ";
  }
  if (details_.length()) {
    error += details_;
  }
  size_t count = 0;
  trace_.Addresses(&count);
  if (count > 0) {
    std::ostringstream ostream;
    trace_.OutputToStream(&ostream);
    error += "\n";
    error += ostream.str();
  }
  return error;
}

ErrorCode Error::code() const {
  return code_;
}

const std::string& Error::details() const {
  return details_;
}

const base::debug::StackTrace& Error::trace() const {
  return trace_;
}

}  // namespace webdriver
