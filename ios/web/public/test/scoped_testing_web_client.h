// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_SCOPED_TESTING_WEB_CLIENT_H_
#define IOS_WEB_PUBLIC_TEST_SCOPED_TESTING_WEB_CLIENT_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace web {

class WebClient;

// Helper class to register a WebClient during unit testing.
class ScopedTestingWebClient {
 public:
  explicit ScopedTestingWebClient(scoped_ptr<WebClient> web_client);
  ~ScopedTestingWebClient();

  WebClient* Get();

 private:
  scoped_ptr<WebClient> web_client_;
  WebClient* original_web_client_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_SCOPED_TESTING_WEB_CLIENT_H_
