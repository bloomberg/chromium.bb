// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_ERROR_CODES_H_
#define CHROME_TEST_WEBDRIVER_ERROR_CODES_H_

namespace webdriver {
// These are the error codes defined in the WebDriver wire protcol. For more
// information, see:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#Response_Status_Codes
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
  kSeeOther = 303,
  kBadRequest = 400,
  kSessionNotFound = 404,
  kMethodNotAllowed = 405,
  kInternalServerError = 500,
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_ERROR_CODES_H_

