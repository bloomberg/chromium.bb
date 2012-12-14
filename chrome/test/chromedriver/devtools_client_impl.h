// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/devtools_client.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "googleurl/src/gurl.h"

namespace base {
class DictionaryValue;
}

class Status;
class SyncWebSocket;

class DevToolsClientImpl : public DevToolsClient {
 public:
  DevToolsClientImpl(const SyncWebSocketFactory& factory,
                     const std::string& url);
  virtual ~DevToolsClientImpl();

  // Overridden from DevToolsClient:
  virtual Status SendCommand(const std::string& method,
                             const base::DictionaryValue& params) OVERRIDE;
  virtual Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result) OVERRIDE;

 private:
  Status SendCommandInternal(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result);
  scoped_ptr<SyncWebSocket> socket_;
  GURL url_;
  bool connected_;
  int next_id_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsClientImpl);
};

#endif  // CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_IMPL_H_
