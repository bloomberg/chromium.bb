// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_AUTOUPDATE_INTERCEPTOR_H_
#define CHROME_BROWSER_EXTENSIONS_AUTOUPDATE_INTERCEPTOR_H_
#pragma once

#include <map>
#include <string>

#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

// This url request interceptor lets us respond to localhost http request urls
// with the contents of files on disk for use in tests.
class AutoUpdateInterceptor
    : public net::URLRequest::Interceptor,
      public base::RefCountedThreadSafe<AutoUpdateInterceptor> {
 public:
  AutoUpdateInterceptor();

  // When computing matches, this ignores query parameters (since the autoupdate
  // fetch code appends a bunch of them to manifest fetches).
  virtual net::URLRequestJob* MaybeIntercept(net::URLRequest* request);

  // When requests for |url| arrive, respond with the contents of |path|. The
  // hostname of |url| must be "localhost" to avoid DNS lookups, and the scheme
  // must be "http" so MaybeIntercept can ignore "chrome" and other schemes.
  // Also, the match for |url| will ignore any query parameters.
  void SetResponse(const std::string url, const FilePath& path);

  // A helper function to call SetResponse on the I/O thread.
  void SetResponseOnIOThread(const std::string url, const FilePath& path);

 private:
  friend class base::RefCountedThreadSafe<AutoUpdateInterceptor>;

  virtual ~AutoUpdateInterceptor();

  std::map<GURL, FilePath> responses_;

  DISALLOW_COPY_AND_ASSIGN(AutoUpdateInterceptor);
};

#endif  // CHROME_BROWSER_EXTENSIONS_AUTOUPDATE_INTERCEPTOR_H_
