// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_STRING_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_STRING_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/net/url_fetcher.h"

// This class is used to fetch an URL and store result as a string.
class StringFetcher : public URLFetcher::Delegate {
 public:
  // Initiates URL fetch.
  explicit StringFetcher(const std::string& url);

  const std::string& result() const { return result_; }
  int response_code() const { return response_code_; }
  bool succeeded() const { return response_code_ == 200; }

 private:
  // Overriden from URLFetcher::Delegate:
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
  // Timer notification handler.
  void OnTimeoutElapsed();

  // URLFetcher instance.
  scoped_ptr<URLFetcher> url_fetcher_;

  // Fetch result.
  std::string result_;

  // Received HTTP response code.
  int response_code_;

  DISALLOW_COPY_AND_ASSIGN(StringFetcher);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_STRING_FETCHER_H_
