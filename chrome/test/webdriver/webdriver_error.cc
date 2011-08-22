// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_error.h"

#include <sstream>

namespace webdriver {

namespace {

// Returns the string equivalent of the given |ErrorCode|.
const char* DefaultMessageForErrorCode(ErrorCode code) {
  switch (code) {
    case kSuccess:
      return "Success";
    case kNoSuchElement:
      return "The element could not be found";
    case kNoSuchFrame:
      return "The frame could not be found";
    case kUnknownCommand:
      return "Unknown command";
    case kStaleElementReference:
      return "Element reference is invalid";
    case kElementNotVisible:
      return "Element is not visible";
    case kInvalidElementState:
      return "Element is in an invalid state";
    case kUnknownError:
      return "Unknown error";
    case kElementNotSelectable:
      return "Element is not selectable";
    case kXPathLookupError:
      return "XPath lookup error";
    case kNoSuchWindow:
      return "The window could not be found";
    case kInvalidCookieDomain:
      return "The cookie domain is invalid";
    case kUnableToSetCookie:
      return "Unable to set cookie";
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

std::string Error::GetMessage() const {
  std::string msg;
  if (details_.length())
    msg = details_;
  else
    msg = DefaultMessageForErrorCode(code_);

  // Only include a stacktrace on Linux. Windows and Mac have all symbols
  // stripped in release builds.
#if defined(OS_LINUX)
  size_t count = 0;
  trace_.Addresses(&count);
  if (count > 0) {
    std::ostringstream ostream;
    trace_.OutputToStream(&ostream);
    msg += "\n";
    msg += ostream.str();
  }
#endif
  return msg;
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
