// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_RESPONSE_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_RESPONSE_HANDLER_H_
#pragma once

#include <string>

#include "chrome/common/net/url_fetcher.h"

class GURL;

namespace chromeos {

// The success code specified by the HTTP spec.
extern const int kHttpSuccess;

class AuthResponseHandler {
 public:
  AuthResponseHandler() {}
  virtual ~AuthResponseHandler() {}

  // True if this object can handle responses from |url|, false otherwise.
  virtual bool CanHandle(const GURL& url) = 0;

  // Caller takes ownership of return value.
  // Takes in |to_process|, creates an appropriate URLFetcher to handle
  // the next step, sets |catcher| to get called back when that fetcher is done.
  // Starts the fetch and returns the fetcher, so the the caller can handle
  // the object lifetime.
  virtual URLFetcher* Handle(const std::string& to_process,
                             URLFetcher::Delegate* catcher) = 0;

  // The URLs for different calls in the Google Accounts programmatic login API.
  static const char kClientLoginUrl[];
  static const char kIssueAuthTokenUrl[];
  static const char kTokenAuthUrl[];
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_RESPONSE_HANDLER_H_
