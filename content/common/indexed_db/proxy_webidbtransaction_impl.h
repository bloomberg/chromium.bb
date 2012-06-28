// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBTRANSACTION_IMPL_H_
#define CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBTRANSACTION_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBTransaction.h"

namespace WebKit {
class WebIDBObjectStore;
class WebIDBTransactionCallbacks;
class WebString;
}

class RendererWebIDBTransactionImpl : public WebKit::WebIDBTransaction {
 public:
  explicit RendererWebIDBTransactionImpl(int32 idb_transaction_id);
  virtual ~RendererWebIDBTransactionImpl();

  virtual int mode() const;
  virtual WebKit::WebIDBObjectStore* objectStore(const WebKit::WebString& name,
                                                 WebKit::WebExceptionCode&);
  virtual void commit();
  virtual void abort();
  virtual void didCompleteTaskEvents();
  virtual void setCallbacks(WebKit::WebIDBTransactionCallbacks*);

  int id() const { return idb_transaction_id_; }

 private:
  int32 idb_transaction_id_;
};

#endif  // CONTENT_COMMON_INDEXED_DB_PROXY_WEBIDBTRANSACTION_IMPL_H_
