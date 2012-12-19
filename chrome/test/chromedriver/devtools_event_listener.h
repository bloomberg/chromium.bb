// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_DEVTOOLS_EVENT_LISTENER_H_
#define CHROME_TEST_CHROMEDRIVER_DEVTOOLS_EVENT_LISTENER_H_

#include <string>

namespace base {
class DictionaryValue;
}

// Listens to WebKit Inspector events.
class DevToolsEventListener {
 public:
  virtual ~DevToolsEventListener() {}

  virtual void OnEvent(const std::string& method,
                       const base::DictionaryValue& params) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_DEVTOOLS_EVENT_LISTENER_H_
