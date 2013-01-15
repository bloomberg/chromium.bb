// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBTRANSACTION_IMPL_H_
#define CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBTRANSACTION_IMPL_H_

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransaction.h"

namespace WebKit {
class WebIDBObjectStore;
class WebIDBTransactionCallbacks;
class WebString;
}

namespace content {

class RendererWebIDBTransactionImpl : public WebKit::WebIDBTransaction {
 public:
  explicit RendererWebIDBTransactionImpl(int32 ipc_transaction_id);
  virtual ~RendererWebIDBTransactionImpl();

  virtual void commit();
  virtual void abort();
  virtual void setCallbacks(WebKit::WebIDBTransactionCallbacks*);

  int ipc_id() const { return ipc_transaction_id_; }

 private:
  int32 ipc_transaction_id_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBTRANSACTION_IMPL_H_
