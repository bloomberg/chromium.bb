// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_INTERCEPTOR_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_INTERCEPTOR_H_

#include "net/url_request/url_request.h"

// Intercepts requests matched by a privacy blacklist.
class BlacklistInterceptor : public URLRequest::Interceptor {
 public:
  BlacklistInterceptor();
  ~BlacklistInterceptor();

  // URLRequest::Interceptor:
  virtual URLRequestJob* MaybeIntercept(URLRequest* request);

 private:
  DISALLOW_COPY_AND_ASSIGN(BlacklistInterceptor);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_INTERCEPTOR_H_
