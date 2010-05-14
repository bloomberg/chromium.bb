// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"

class IndexedDBDispatcherHost;

class IndexedDBCallbacks : public WebKit::WebIDBCallbacks {
 public:
  IndexedDBCallbacks(IndexedDBDispatcherHost* parent, int32 response_id);
  virtual ~IndexedDBCallbacks();

  // We define default versions of these functions which simply DCHECK.
  // For each WebIDBCallbacks implementation, there should only be one success
  // callback that can possibly be called.
  virtual void onSuccess(WebKit::WebIDBDatabase* idb_database);
  virtual void onSuccess(const WebKit::WebSerializedScriptValue& value);

 protected:
  IndexedDBDispatcherHost* parent() { return parent_.get(); }
  int32 response_id() const { return response_id_; }

 private:
  scoped_refptr<IndexedDBDispatcherHost> parent_;
  int32 response_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_

