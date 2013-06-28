// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_WEBIDBFACTORY_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"

namespace content {

class IndexedDBFactory;
class IndexedDBCallbacksBase;
class IndexedDBDatabaseCallbacks;

class WebIDBFactoryImpl {
 public:
  WebIDBFactoryImpl();
  virtual ~WebIDBFactoryImpl();

  virtual void getDatabaseNames(IndexedDBCallbacksBase* callbacks,
                                const string16& database_identifier,
                                const string16& data_dir);
  virtual void open(const string16& name,
                    long long version,
                    long long transaction_id,
                    IndexedDBCallbacksBase* callbacks,
                    IndexedDBDatabaseCallbacks* database_callbacks,
                    const string16& database_identifier,
                    const string16& data_dir);
  virtual void deleteDatabase(const string16& name,
                              IndexedDBCallbacksBase* callbacks,
                              const string16& database_identifier,
                              const string16& data_dir);

 private:
  scoped_refptr<IndexedDBFactory> idb_factory_backend_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
