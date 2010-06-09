// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "chrome/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBDatabaseError.h"


template <class WebObjectType, typename SuccessMsgType, typename ErrorMsgType>
class IndexedDBCallbacks : public WebKit::WebIDBCallbacks {
 public:
  IndexedDBCallbacks(
      IndexedDBDispatcherHost* dispatcher_host, int32 response_id)
      : dispatcher_host_(dispatcher_host), response_id_(response_id) { }

  virtual void onError(const WebKit::WebIDBDatabaseError& error) {
    dispatcher_host_->Send(new ErrorMsgType(
        response_id_, error.code(), error.message()));
  }

  virtual void onSuccess(WebObjectType* idb_object) {
    int32 object_id = dispatcher_host_->Add(idb_object);
    dispatcher_host_->Send(new SuccessMsgType(response_id_, object_id));
  }

 private:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int32 response_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBCallbacks);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_CALLBACKS_H_

