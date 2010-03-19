// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTH_RESPONSE_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTH_RESPONSE_HANDLER_H_

#include "chrome/browser/chromeos/login/auth_response_handler.h"

#include <string>

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;
class URLFetcher;

class MockAuthResponseHandler : public AuthResponseHandler {
 public:
  MockAuthResponseHandler() {}
  virtual ~MockAuthResponseHandler() {}

  MOCK_METHOD1(CanHandle, bool(const GURL& url));
  MOCK_METHOD2(Handle, URLFetcher*(const std::string& to_process,
                                   URLFetcher::Delegate* catcher));
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTH_RESPONSE_HANDLER_H_
