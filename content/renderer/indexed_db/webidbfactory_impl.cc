// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/indexed_db/webidbfactory_impl.h"

#include "base/memory/ptr_util.h"
#include "content/renderer/indexed_db/indexed_db_callbacks_impl.h"
#include "content/renderer/indexed_db/indexed_db_database_callbacks_impl.h"
#include "content/renderer/storage_util.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"

using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseCallbacks;
using blink::WebSecurityOrigin;
using blink::WebString;
using indexed_db::mojom::CallbacksAssociatedPtrInfo;
using indexed_db::mojom::DatabaseCallbacksAssociatedPtrInfo;
using indexed_db::mojom::FactoryPtr;
using indexed_db::mojom::FactoryPtrInfo;

namespace content {

WebIDBFactoryImpl::WebIDBFactoryImpl(FactoryPtrInfo factory_info)
    : factory_(std::move(factory_info)) {}

WebIDBFactoryImpl::~WebIDBFactoryImpl() = default;

void WebIDBFactoryImpl::GetDatabaseNames(
    WebIDBCallbacks* callbacks,
    const WebSecurityOrigin& origin,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), IndexedDBCallbacksImpl::kNoTransaction,
      nullptr, std::move(task_runner));
  factory_->GetDatabaseNames(GetCallbacksProxy(std::move(callbacks_impl)),
                             url::Origin(origin));
}

void WebIDBFactoryImpl::Open(
    const WebString& name,
    long long version,
    long long transaction_id,
    WebIDBCallbacks* callbacks,
    WebIDBDatabaseCallbacks* database_callbacks,
    const WebSecurityOrigin& origin,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), transaction_id, nullptr, task_runner);
  auto database_callbacks_impl =
      std::make_unique<IndexedDBDatabaseCallbacksImpl>(
          base::WrapUnique(database_callbacks));
  factory_->Open(GetCallbacksProxy(std::move(callbacks_impl)),
                 GetDatabaseCallbacksProxy(std::move(database_callbacks_impl)),
                 url::Origin(origin), name.Utf16(), version, transaction_id);
}

void WebIDBFactoryImpl::DeleteDatabase(
    const WebString& name,
    WebIDBCallbacks* callbacks,
    const WebSecurityOrigin& origin,
    bool force_close,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto callbacks_impl = std::make_unique<IndexedDBCallbacksImpl>(
      base::WrapUnique(callbacks), IndexedDBCallbacksImpl::kNoTransaction,
      nullptr, std::move(task_runner));
  factory_->DeleteDatabase(GetCallbacksProxy(std::move(callbacks_impl)),
                           url::Origin(origin), name.Utf16(), force_close);
}

CallbacksAssociatedPtrInfo WebIDBFactoryImpl::GetCallbacksProxy(
    std::unique_ptr<IndexedDBCallbacksImpl> callbacks) {
  CallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request));
  return ptr_info;
}

DatabaseCallbacksAssociatedPtrInfo WebIDBFactoryImpl::GetDatabaseCallbacksProxy(
    std::unique_ptr<IndexedDBDatabaseCallbacksImpl> callbacks) {
  DatabaseCallbacksAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  mojo::MakeStrongAssociatedBinding(std::move(callbacks), std::move(request));
  return ptr_info;
}

}  // namespace content
