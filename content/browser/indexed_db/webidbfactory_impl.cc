// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/webidbfactory_impl.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/indexed_db/indexed_db_callbacks_wrapper.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseError.h"
#include "webkit/base/file_path_string_conversions.h"

using WebKit::WebString;

namespace content {

WebIDBFactoryImpl::WebIDBFactoryImpl()
    : idb_factory_backend_(IndexedDBFactory::Create()) {}

WebIDBFactoryImpl::~WebIDBFactoryImpl() {}

void WebIDBFactoryImpl::getDatabaseNames(IndexedDBCallbacksBase* callbacks,
                                         const WebString& database_identifier,
                                         const WebString& data_dir) {
  idb_factory_backend_->GetDatabaseNames(
      IndexedDBCallbacksWrapper::Create(callbacks),
      database_identifier,
      webkit_base::WebStringToFilePath(data_dir));
}

void WebIDBFactoryImpl::open(const WebString& name,
                             long long version,
                             long long transaction_id,
                             IndexedDBCallbacksBase* callbacks,
                             IndexedDBDatabaseCallbacks* database_callbacks,
                             const WebString& database_identifier,
                             const WebString& data_dir) {
  scoped_refptr<IndexedDBCallbacksWrapper> callbacks_proxy =
      IndexedDBCallbacksWrapper::Create(callbacks);
  scoped_refptr<IndexedDBDatabaseCallbacksWrapper> database_callbacks_proxy =
      IndexedDBDatabaseCallbacksWrapper::Create(database_callbacks);

  callbacks_proxy->SetDatabaseCallbacks(database_callbacks_proxy);
  idb_factory_backend_->Open(name,
                             version,
                             transaction_id,
                             callbacks_proxy.get(),
                             database_callbacks_proxy.get(),
                             database_identifier,
                             webkit_base::WebStringToFilePath(data_dir));
}

void WebIDBFactoryImpl::deleteDatabase(const WebString& name,
                                       IndexedDBCallbacksBase* callbacks,
                                       const WebString& database_identifier,
                                       const WebString& data_dir) {
  idb_factory_backend_->DeleteDatabase(
      name,
      IndexedDBCallbacksWrapper::Create(callbacks),
      database_identifier,
      webkit_base::WebStringToFilePath(data_dir));
}

}  // namespace WebKit
