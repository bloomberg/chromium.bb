// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "content/common/content_export.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_task.h"
#include "webkit/quota/quota_types.h"

namespace content {
class IndexedDBContextImpl;

// A QuotaClient implementation to integrate IndexedDB
// with the quota  management system. This interface is used
// on the IO thread by the quota manager.
class IndexedDBQuotaClient : public quota::QuotaClient,
                             public quota::QuotaTaskObserver {
 public:
  CONTENT_EXPORT IndexedDBQuotaClient(
      base::MessageLoopProxy* tracker_thread,
      IndexedDBContextImpl* indexed_db_context);
  CONTENT_EXPORT virtual ~IndexedDBQuotaClient();

  // QuotaClient method overrides
  virtual ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  virtual void GetOriginUsage(const GURL& origin_url,
                              quota::StorageType type,
                              const GetUsageCallback& callback) OVERRIDE;
  virtual void GetOriginsForType(quota::StorageType type,
                                 const GetOriginsCallback& callback) OVERRIDE;
  virtual void GetOriginsForHost(quota::StorageType type,
                                 const std::string& host,
                                 const GetOriginsCallback& callback) OVERRIDE;
  virtual void DeleteOriginData(const GURL& origin,
                                quota::StorageType type,
                                const DeletionCallback& callback) OVERRIDE;
 private:
  scoped_refptr<base::MessageLoopProxy> webkit_thread_message_loop_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBQuotaClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_QUOTA_CLIENT_H_
