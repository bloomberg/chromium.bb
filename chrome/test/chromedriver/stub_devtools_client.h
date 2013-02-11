// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_STUB_DEVTOOLS_CLIENT_H_
#define CHROME_TEST_CHROMEDRIVER_STUB_DEVTOOLS_CLIENT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/devtools_client.h"

namespace base {
class DictionaryValue;
}
class Status;

class StubDevToolsClient : public DevToolsClient {
 public:
  StubDevToolsClient();
  virtual ~StubDevToolsClient();

  // Overridden from DevToolsClient:
  virtual Status SendCommand(const std::string& method,
                             const base::DictionaryValue& params) OVERRIDE;
  virtual Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result) OVERRIDE;
  virtual void AddListener(DevToolsEventListener* listener) OVERRIDE;
  virtual Status HandleEventsUntil(
      const ConditionalFunc& conditional_func) OVERRIDE;
};

#endif  // CHROME_TEST_CHROMEDRIVER_STUB_DEVTOOLS_CLIENT_H_
