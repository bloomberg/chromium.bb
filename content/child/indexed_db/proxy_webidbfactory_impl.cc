// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/proxy_webidbfactory_impl.h"

#include "content/child/thread_safe_sender.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"

using WebKit::WebIDBCallbacks;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBDatabaseCallbacks;
using WebKit::WebString;

namespace content {

RendererWebIDBFactoryImpl::RendererWebIDBFactoryImpl(
    ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender) {
}

RendererWebIDBFactoryImpl::~RendererWebIDBFactoryImpl() {
}

void RendererWebIDBFactoryImpl::getDatabaseNames(
    WebIDBCallbacks* callbacks,
    const WebString& database_identifier) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBFactoryGetDatabaseNames(callbacks,
                                                database_identifier.utf8());
}

void RendererWebIDBFactoryImpl::open(
    const WebString& name,
    long long version,
    long long transaction_id,
    WebIDBCallbacks* callbacks,
    WebIDBDatabaseCallbacks* database_callbacks,
    const WebString& database_identifier) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBFactoryOpen(name,
                                    version,
                                    transaction_id,
                                    callbacks,
                                    database_callbacks,
                                    database_identifier.utf8());
}

void RendererWebIDBFactoryImpl::deleteDatabase(
    const WebString& name,
    WebIDBCallbacks* callbacks,
    const WebString& database_identifier) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBFactoryDeleteDatabase(
      name, callbacks, database_identifier.utf8());
}

}  // namespace content
