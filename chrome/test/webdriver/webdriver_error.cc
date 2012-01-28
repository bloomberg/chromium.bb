// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_error.h"

#include <sstream>

#include "chrome/common/automation_constants.h"

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
    case kUnexpectedAlertOpen:
      return "An open modal dialog blocked the operation";
    case kNoAlertOpenError:
      return "No JavaScript modal dialog is open";
    default:
      return "<unknown>";
  }
}

}  // namespace

// static
Error* Error::FromAutomationError(const automation::Error& error) {
  ErrorCode code = kUnknownError;
  switch (error.code()) {
    case automation::kNoJavaScriptModalDialogOpen:
      code = kNoAlertOpenError;
      break;
    case automation::kBlockedByModalDialog:
      code = kUnexpectedAlertOpen;
      break;
    case automation::kInvalidId:
      code = kNoSuchWindow;
    default:
      break;
  }

  // In Chrome 17 and before, the automation error was just a string.
  // Compare against some strings that correspond to webdriver errors.
  // TODO(kkania): Remove these comparisons when Chrome 17 is unsupported.
  if (code == kUnknownError) {
    if (error.message() ==
            "Command cannot be performed because a modal dialog is active" ||
        error.message() ==
            "Could not complete script execution because a modal "
                "dialog is active") {
      code = kUnexpectedAlertOpen;
    } else if (error.message() == "No modal dialog is showing" ||
               error.message() == "No JavaScript modal dialog is showing") {
      code = kNoAlertOpenError;
    }
  }

  // If the automation error code did not refer to a webdriver error code
  // (besides unknown), include the error message from chrome. Otherwise,
  // include the webdriver error code and the webdriver error message.
  if (code == kUnknownError) {
    return new Error(code, error.message());
  } else {
    return new Error(code);
  }
}

Error::Error(ErrorCode code)
    : code_(code),
      details_(DefaultMessageForErrorCode(code)) {
}

Error::Error(ErrorCode code, const std::string& details)
    : code_(code), details_(details) {
}

Error::~Error() {
}

void Error::AddDetails(const std::string& details) {
  details_ = details + ";\n " + details_;
}

ErrorCode Error::code() const {
  return code_;
}

const std::string& Error::details() const {
  return details_;
}

}  // namespace webdriver
