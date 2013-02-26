// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/stub_devtools_client.h"

#include "base/values.h"
#include "chrome/test/chromedriver/status.h"

StubDevToolsClient::StubDevToolsClient() {}

StubDevToolsClient::~StubDevToolsClient() {}

Status StubDevToolsClient::SendCommand(const std::string& method,
                                       const base::DictionaryValue& params) {
  scoped_ptr<base::DictionaryValue> result;
  return SendCommandAndGetResult(method, params, &result);
}

Status StubDevToolsClient::SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result) {
  return Status(kOk);
}

void StubDevToolsClient::AddListener(DevToolsEventListener* listener) {}

Status StubDevToolsClient::HandleEventsUntil(
      const ConditionalFunc& conditional_func) {
  return Status(kOk);
}
