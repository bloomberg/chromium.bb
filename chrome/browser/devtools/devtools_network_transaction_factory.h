// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_TRANSACTION_FACTORY_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_TRANSACTION_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/request_priority.h"
#include "net/http/http_transaction_factory.h"

class DevToolsNetworkController;

namespace net {
class HttpCache;
class HttpNetworkSession;
class HttpTransaction;
}

// NetworkTransactionFactory wraps HttpNetworkTransactions.
class DevToolsNetworkTransactionFactory : public net::HttpTransactionFactory {
 public:
  DevToolsNetworkTransactionFactory(
      DevToolsNetworkController* controller,
      net::HttpNetworkSession* session);
  virtual ~DevToolsNetworkTransactionFactory();

  // net::HttpTransactionFactory methods:
  virtual int CreateTransaction(
      net::RequestPriority priority,
      scoped_ptr<net::HttpTransaction>* trans) OVERRIDE;
  virtual net::HttpCache* GetCache() OVERRIDE;
  virtual net::HttpNetworkSession* GetSession() OVERRIDE;

 private:
  DevToolsNetworkController* controller_;
  scoped_ptr<net::HttpTransactionFactory> network_layer_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkTransactionFactory);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_TRANSACTION_FACTORY_H_
