// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_WEBIDBFACTORY_IMPL_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebIDBFactory.h"

namespace content {

class IndexedDBFactory;

class WebIDBFactoryImpl : public WebKit::WebIDBFactory {
 public:
  WebIDBFactoryImpl();
  virtual ~WebIDBFactoryImpl();

  virtual void getDatabaseNames(WebKit::WebIDBCallbacks* callbacks,
                                const WebKit::WebString& database_identifier,
                                const WebKit::WebString& data_dir);
  virtual void open(const WebKit::WebString& name,
                    long long version,
                    long long transaction_id,
                    WebKit::WebIDBCallbacks* callbacks,
                    WebKit::WebIDBDatabaseCallbacks* database_callbacks,
                    const WebKit::WebString& database_identifier,
                    const WebKit::WebString& data_dir);
  virtual void deleteDatabase(const WebKit::WebString& name,
                              WebKit::WebIDBCallbacks* callbacks,
                              const WebKit::WebString& database_identifier,
                              const WebKit::WebString& data_dir);

 private:
  scoped_refptr<IndexedDBFactory> idb_factory_backend_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
