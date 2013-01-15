// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_STATUS_H_
#define CHROME_TEST_CHROMEDRIVER_STATUS_H_

#include <string>

// WebDriver standard status codes.
enum StatusCode {
  kOk = 0,
  kNoSuchElement = 7,
  kUnknownCommand = 9,
  kUnknownError = 13,
  kXPathLookupError = 19,
  kInvalidSelector = 32,
  kSessionNotCreatedException = 33,
  // Chrome-specific status codes.
  kNoSuchSession = 100,
  kChromeNotReachable,
};

// Represents a WebDriver status, which may be an error or ok.
class Status {
 public:
  explicit Status(StatusCode code);
  Status(StatusCode code, const std::string& details);
  Status(StatusCode code, const Status& cause);
  Status(StatusCode code, const std::string& details, const Status& cause);
  ~Status();

  bool IsOk() const;
  bool IsError() const;

  StatusCode code() const;

  const std::string& message() const;

 private:
  StatusCode code_;
  std::string msg_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_STATUS_H_
