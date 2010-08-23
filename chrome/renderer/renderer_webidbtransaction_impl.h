// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBIDBTRANSACTION_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBIDBTRANSACTION_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBTransaction.h"

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
  virtual WebKit::WebIDBObjectStore* objectStore(const WebKit::WebString& name);
  virtual void abort();
  virtual int id() const;
  virtual void setCallbacks(WebKit::WebIDBTransactionCallbacks*);

 private:
  int32 idb_transaction_id_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBIDBTRANSACTION_IMPL_H_
