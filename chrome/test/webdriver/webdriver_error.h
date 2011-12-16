// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_WEBDRIVER_ERROR_H_
#define CHROME_TEST_WEBDRIVER_WEBDRIVER_ERROR_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace automation {
class Error;
}

namespace webdriver {

// Error codes defined by the WebDriver wire protcol.
// If you add a code here, don't forget to add it to |ErrorCodeToString|.
enum ErrorCode {
  kSuccess = 0,
  kNoSuchElement = 7,
  kNoSuchFrame = 8,
  kUnknownCommand = 9,
  kStaleElementReference = 10,
  kElementNotVisible = 11,
  kInvalidElementState = 12,
  kUnknownError = 13,
  kElementNotSelectable = 15,
  kXPathLookupError = 19,
  kNoSuchWindow = 23,
  kInvalidCookieDomain = 24,
  kUnableToSetCookie = 25,
  kUnexpectedAlertOpen = 26,
  kNoAlertOpenError = 27,

  // HTTP status codes.
  kSeeOther = 303,
  kBadRequest = 400,
  kSessionNotFound = 404,
  kMethodNotAllowed = 405,
  kInternalServerError = 500,
};

// Represents a WebDriver error and the context within which the error occurred.
class Error {
 public:
  static Error* FromAutomationError(const automation::Error& error);

  explicit Error(ErrorCode code);

  Error(ErrorCode code, const std::string& details);

  virtual ~Error();

  void AddDetails(const std::string& details);

  ErrorCode code() const;
  const std::string& details() const;

 private:
  ErrorCode code_;
  std::string details_;

  DISALLOW_COPY_AND_ASSIGN(Error);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_WEBDRIVER_ERROR_H_
