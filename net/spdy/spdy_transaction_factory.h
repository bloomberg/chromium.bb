// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_TRANSACTION_FACTORY_H__
#define NET_SPDY_SPDY_TRANSACTION_FACTORY_H__

#include "net/http/http_transaction_factory.h"
#include "net/spdy/spdy_network_transaction.h"

namespace net {

class SpdyTransactionFactory : public HttpTransactionFactory {
 public:
  explicit SpdyTransactionFactory(HttpNetworkSession* session)
     : session_(session) {
  }
  virtual ~SpdyTransactionFactory() {}

  // HttpTransactionFactory Interface.
  virtual HttpTransaction* CreateTransaction() {
    return new SpdyNetworkTransaction(session_);
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

#endif  // NET_SPDY_SPDY_TRANSACTION_FACTORY_H__
