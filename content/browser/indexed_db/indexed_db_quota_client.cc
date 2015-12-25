// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_quota_client.h"

#include <stdint.h>

#include <vector>

#include "base/logging.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_util.h"
#include "storage/browser/database/database_util.h"

using storage::QuotaClient;
using storage::DatabaseUtil;

namespace content {
namespace {

storage::QuotaStatusCode DeleteOriginDataOnIndexedDBThread(
    IndexedDBContextImpl* context,
    const GURL& origin) {
  context->DeleteForOrigin(origin);
  return storage::kQuotaStatusOk;
}

int64_t GetOriginUsageOnIndexedDBThread(IndexedDBContextImpl* context,
                                        const GURL& origin) {
  DCHECK(context->TaskRunner()->RunsTasksOnCurrentThread());
  return context->GetOriginDiskUsage(origin);
}

void GetAllOriginsOnIndexedDBThread(IndexedDBContextImpl* context,
                                    std::set<GURL>* origins_to_return) {
  DCHECK(context->TaskRunner()->RunsTasksOnCurrentThread());
  std::vector<GURL> all_origins = context->GetAllOrigins();
  origins_to_return->insert(all_origins.begin(), all_origins.end());
}

void DidGetOrigins(const IndexedDBQuotaClient::GetOriginsCallback& callback,
                   const std::set<GURL>* origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(*origins);
}

void GetOriginsForHostOnIndexedDBThread(IndexedDBContextImpl* context,
                                        const std::string& host,
                                        std::set<GURL>* origins_to_return) {
  DCHECK(context->TaskRunner()->RunsTasksOnCurrentThread());
  std::vector<GURL> all_origins = context->GetAllOrigins();
  for (const auto& origin_url : all_origins) {
    if (host == net::GetHostOrSpecFromURL(origin_url))
      origins_to_return->insert(origin_url);
  }
}

}  // namespace

// IndexedDBQuotaClient --------------------------------------------------------

IndexedDBQuotaClient::IndexedDBQuotaClient(
    IndexedDBContextImpl* indexed_db_context)
    : indexed_db_context_(indexed_db_context) {}

IndexedDBQuotaClient::~IndexedDBQuotaClient() {}

QuotaClient::ID IndexedDBQuotaClient::id() const { return kIndexedDatabase; }

void IndexedDBQuotaClient::OnQuotaManagerDestroyed() { delete this; }

void IndexedDBQuotaClient::GetOriginUsage(const GURL& origin_url,
                                          storage::StorageType type,
                                          const GetUsageCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_.get());

  // IndexedDB is in the temp namespace for now.
  if (type != storage::kStorageTypeTemporary) {
    callback.Run(0);
    return;
  }

  // No task runner means unit test; no cleanup necessary.
  if (!indexed_db_context_->TaskRunner()) {
    callback.Run(0);
    return;
  }

  base::PostTaskAndReplyWithResult(
      indexed_db_context_->TaskRunner(),
      FROM_HERE,
      base::Bind(
          &GetOriginUsageOnIndexedDBThread, indexed_db_context_, origin_url),
      callback);
}

void IndexedDBQuotaClient::GetOriginsForType(
    storage::StorageType type,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_.get());

  // All databases are in the temp namespace for now.
  if (type != storage::kStorageTypeTemporary) {
    callback.Run(std::set<GURL>());
    return;
  }

  // No task runner means unit test; no cleanup necessary.
  if (!indexed_db_context_->TaskRunner()) {
    callback.Run(std::set<GURL>());
    return;
  }

  std::set<GURL>* origins_to_return = new std::set<GURL>();
  indexed_db_context_->TaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetAllOriginsOnIndexedDBThread,
                 indexed_db_context_,
                 base::Unretained(origins_to_return)),
      base::Bind(&DidGetOrigins, callback, base::Owned(origins_to_return)));
}

void IndexedDBQuotaClient::GetOriginsForHost(
    storage::StorageType type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(indexed_db_context_.get());

  // All databases are in the temp namespace for now.
  if (type != storage::kStorageTypeTemporary) {
    callback.Run(std::set<GURL>());
    return;
  }

  // No task runner means unit test; no cleanup necessary.
  if (!indexed_db_context_->TaskRunner()) {
    callback.Run(std::set<GURL>());
    return;
  }

  std::set<GURL>* origins_to_return = new std::set<GURL>();
  indexed_db_context_->TaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetOriginsForHostOnIndexedDBThread,
                 indexed_db_context_,
                 host,
                 base::Unretained(origins_to_return)),
      base::Bind(&DidGetOrigins, callback, base::Owned(origins_to_return)));
}

void IndexedDBQuotaClient::DeleteOriginData(const GURL& origin,
                                            storage::StorageType type,
                                            const DeletionCallback& callback) {
  if (type != storage::kStorageTypeTemporary) {
    callback.Run(storage::kQuotaErrorNotSupported);
    return;
  }

  // No task runner means unit test; no cleanup necessary.
  if (!indexed_db_context_->TaskRunner()) {
    callback.Run(storage::kQuotaStatusOk);
    return;
  }

  base::PostTaskAndReplyWithResult(
      indexed_db_context_->TaskRunner(),
      FROM_HERE,
      base::Bind(
          &DeleteOriginDataOnIndexedDBThread, indexed_db_context_, origin),
      callback);
}

bool IndexedDBQuotaClient::DoesSupport(storage::StorageType type) const {
  return type == storage::kStorageTypeTemporary;
}

}  // namespace content
