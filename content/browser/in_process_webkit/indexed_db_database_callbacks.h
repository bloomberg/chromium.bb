// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DATABASE_CALLBACKS_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DATABASE_CALLBACKS_H_
#pragma once

#include "base/ref_counted.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabaseCallbacks.h"

class IndexedDBDispatcherHost;

class IndexedDBDatabaseCallbacks
    : public WebKit::WebIDBDatabaseCallbacks {
 public:
  IndexedDBDatabaseCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                             int database_id);

  virtual ~IndexedDBDatabaseCallbacks();

  virtual void onVersionChange(const WebKit::WebString& requested_version);

 private:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int database_id_;
};

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_INDEXED_DB_DATABASE_CALLBACKS_H_
