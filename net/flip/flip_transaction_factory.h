// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FLIP_FLIP_TRANSACTION_FACTORY_H__
#define NET_FLIP_FLIP_TRANSACTION_FACTORY_H__

#include "net/flip/flip_network_transaction.h"
#include "net/http/http_transaction_factory.h"

namespace net {

class FlipTransactionFactory : public HttpTransactionFactory {
 public:
  explicit FlipTransactionFactory(HttpNetworkSession* session)
     : session_(session) {
  }
  virtual ~FlipTransactionFactory() {}

  // HttpTransactionFactory Interface.
  virtual HttpTransaction* CreateTransaction() {
    return new FlipNetworkTransaction(session_);
  }
  virtual HttpCache* GetCache() {
    return NULL;
  }
  virtual void Suspend(bool suspend) {
  }

 private:
  scoped_refptr<HttpNetworkSession> session_;
};

}  // namespace net

#endif  // NET_FLIP_FLIP_TRANSACTION_FACTORY_H__
