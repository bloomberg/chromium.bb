// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_EVENT_LISTENER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_EVENT_LISTENER_H_

#include <string>

namespace base {
class DictionaryValue;
}

class Status;

// Listens to WebKit Inspector events and DevTools debugger connection.
class DevToolsEventListener {
 public:
  virtual ~DevToolsEventListener() {}

  virtual Status OnConnected() = 0;

  virtual void OnEvent(const std::string& method,
                       const base::DictionaryValue& params) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_EVENT_LISTENER_H_
