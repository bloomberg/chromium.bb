// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/status.h"

namespace {

// Returns the string equivalent of the given |ErrorCode|.
const char* DefaultMessageForStatusCode(StatusCode code) {
  switch (code) {
    case kOk:
      return "ok";
    case kNoSuchElement:
      return "no such element";
    case kUnknownCommand:
      return "unknown command";
    case kStaleElementReference:
      return "stale element reference";
    case kElementNotVisible:
      return "element not visible";
    case kUnknownError:
      return "unknown error";
    case kXPathLookupError:
      return "xpath lookup error";
    case kInvalidSelector:
      return "invalid selector";
    case kSessionNotCreatedException:
      return "session not created exception";
    case kNoSuchSession:
      return "no such session";
    case kChromeNotReachable:
      return "chrome not reachable";
    default:
      return "<unknown>";
  }
}

}  // namespace

Status::Status(StatusCode code)
    : code_(code), msg_(DefaultMessageForStatusCode(code)) {}

Status::Status(StatusCode code, const std::string& details)
    : code_(code),
      msg_(DefaultMessageForStatusCode(code) + std::string(": ") + details) {
}

Status::Status(StatusCode code, const Status& cause)
    : code_(code),
      msg_(DefaultMessageForStatusCode(code) + std::string("\nfrom ") +
           cause.message()) {}

Status::Status(StatusCode code,
               const std::string& details,
               const Status& cause)
    : code_(code),
      msg_(DefaultMessageForStatusCode(code) + std::string(": ") + details +
           "\nfrom " + cause.message()) {
}

Status::~Status() {}

bool Status::IsOk() const {
  return code_ == kOk;
}

bool Status::IsError() const {
  return code_ != kOk;
}

StatusCode Status::code() const {
  return code_;
}

const std::string& Status::message() const {
  return msg_;
}
