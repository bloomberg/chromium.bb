// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TEST_URL_REQUEST_POST_INTERCEPTOR_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TEST_URL_REQUEST_POST_INTERCEPTOR_H_

#include <string>
#include <vector>
#include "base/basictypes.h"

namespace base {
class FilePath;
}

namespace net {
class URLRequest;
}

// Intercepts POST requests, counts them, and captures the body of the requests.
// Optionally, for each request, it can return a canned response from a
// given file. The class maintains a queue of paths to serve responses from,
// and returns one and only one response for each request that it intercepts.
class URLRequestPostInterceptor {
 public:
  URLRequestPostInterceptor(const std::string& scheme,
                            const std::string& hostname);
  ~URLRequestPostInterceptor();

 // Returns how many requests have been intercepted.
 int GetHitCount() const;

 // Returns the requests that have been intercepted.
 std::vector<std::string> GetRequests() const;

 // Returns all requests as a string for debugging purposes.
 std::string GetRequestsAsString() const;

 // Adds a response to its queue of responses to serve.
 // TODO(sorin): implement this.
 void AddResponse(const base::FilePath& path);

 private:
  class Delegate;

  const std::string scheme_;
  const std::string hostname_;

  // After creation, |delegate_| lives on the IO thread and it is owned by
  // a URLRequestFilter after registration. A task to unregister it and
  // implicitly destroy it is posted from ~URLRequestPostInterceptor().
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestPostInterceptor);
};

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TEST_URL_REQUEST_POST_INTERCEPTOR_H_
