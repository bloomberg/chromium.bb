// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_H_
#define CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

class Status;
class SyncWebSocket;
class URLRequestContextGetter;

// A DevTools client of a single DevTools debugger.
class DevToolsClient {
 public:
  DevToolsClient(URLRequestContextGetter* context_getter,
                 const std::string& url);
  ~DevToolsClient();

  Status SendCommand(const std::string& method,
                     const base::DictionaryValue& params);

 private:
  scoped_refptr<URLRequestContextGetter> context_getter_;
  std::string url_;
  scoped_ptr<SyncWebSocket> socket_;
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsClient);
};

#endif  // CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_H_
