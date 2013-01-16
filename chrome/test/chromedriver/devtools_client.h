// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_H_
#define CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

class DevToolsEventListener;
class Status;

// A DevTools client of a single DevTools debugger.
class DevToolsClient {
 public:
  virtual ~DevToolsClient() {}

  virtual Status SendCommand(const std::string& method,
                             const base::DictionaryValue& params) = 0;
  virtual Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result) = 0;

  virtual void AddListener(DevToolsEventListener* listener) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_H_
