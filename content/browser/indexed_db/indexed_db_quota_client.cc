// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_quota_client.h"

#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_util.h"
#include "webkit/database/database_util.h"

using quota::QuotaClient;
using webkit_database::DatabaseUtil;

namespace content {
namespace {

quota::QuotaStatusCode DeleteOriginDataOnWebKitThread(
    IndexedDBContextImpl* context,
    const GURL& origin) {
  context->DeleteForOrigin(origin);
  return quota::kQuotaStatusOk;
}

int64 GetOriginUsageOnWebKitThread(
    IndexedDBContextImpl* context,
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  return context->GetOriginDiskUsage(origin);
}

void GetAllOriginsOnWebKitThread(
    IndexedDBContextImpl* context,
    std::set<GURL>* origins_to_return) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  std::vector<GURL> all_origins = context->GetAllOrigins();
  origins_to_return->insert(all_origins.begin(), all_origins.end());
}

void DidGetOrigins(
    const IndexedDBQuotaClient::GetOriginsCallback& callback,
    const std::set<GURL>* origins,
    quota::StorageType storage_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  callback.Run(*origins, storage_type);
}

void GetOriginsForHostOnWebKitThread(
    IndexedDBContextImpl* context,
    const std::string& host,
    std::set<GURL>* origins_to_return) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  std::vector<GURL> all_origins = context->GetAllOrigins();
  for (std::vector<GURL>::const_iterator iter = all_origins.begin();
       iter != all_origins.end(); ++iter) {
    if (host == net::GetHostOrSpecFromURL(*iter))
      origins_to_return->insert(*iter);
  }
}

}  // namespace

// IndexedDBQuotaClient --------------------------------------------------------

IndexedDBQuotaClient::IndexedDBQuotaClient(
    base::MessageLoopProxy* webkit_thread_message_loop,
    IndexedDBContextImpl* indexed_db_context)
    : webkit_thread_message_loop_(webkit_thread_message_loop),
      indexed_db_context_(indexed_db_context) {
}

IndexedDBQuotaClient::~IndexedDBQuotaClient() {
}

QuotaClient::ID IndexedDBQuotaClient::id() const {
  return kIndexedDatabase;
}

void IndexedDBQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

void IndexedDBQuotaClient::GetOriginUsage(
    const GURL& origin_url,
    quota::StorageType type,
    const GetUsageCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_.get());

  // IndexedDB is in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(0);
    return;
  }

  base::PostTaskAndReplyWithResult(
      webkit_thread_message_loop_,
      FROM_HERE,
      base::Bind(&GetOriginUsageOnWebKitThread,
                 indexed_db_context_,
                 origin_url),
      callback);
}

void IndexedDBQuotaClient::GetOriginsForType(
    quota::StorageType type,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_.get());

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(std::set<GURL>(), type);
    return;
  }

  std::set<GURL>* origins_to_return = new std::set<GURL>();
  webkit_thread_message_loop_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetAllOriginsOnWebKitThread,
                 indexed_db_context_,
                 base::Unretained(origins_to_return)),
      base::Bind(&DidGetOrigins,
                 callback,
                 base::Owned(origins_to_return),
                 type));
}

void IndexedDBQuotaClient::GetOriginsForHost(
    quota::StorageType type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_.get());

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(std::set<GURL>(), type);
    return;
  }

  std::set<GURL>* origins_to_return = new std::set<GURL>();
  webkit_thread_message_loop_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetOriginsForHostOnWebKitThread,
                 indexed_db_context_,
                 host,
                 base::Unretained(origins_to_return)),
      base::Bind(&DidGetOrigins,
                 callback,
                 base::Owned(origins_to_return),
                 type));
}

void IndexedDBQuotaClient::DeleteOriginData(
    const GURL& origin,
    quota::StorageType type,
    const DeletionCallback& callback) {
  if (type != quota::kStorageTypeTemporary) {
    callback.Run(quota::kQuotaErrorNotSupported);
    return;
  }

  base::PostTaskAndReplyWithResult(
      webkit_thread_message_loop_,
      FROM_HERE,
      base::Bind(&DeleteOriginDataOnWebKitThread,
                 indexed_db_context_,
                 origin),
      callback);
}

}  // namespace content
