// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWER_IN_PROCESS_WEBKIT_QUOTA_CLIENT_H_
#define CONTENT_BROWER_IN_PROCESS_WEBKIT_QUOTA_CLIENT_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "content/common/content_export.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_task.h"
#include "webkit/quota/quota_types.h"

class IndexedDBContext;

// A QuotaClient implementation to integrate IndexedDB
// with the quota  management system. This interface is used
// on the IO thread by the quota manager.
class IndexedDBQuotaClient : public quota::QuotaClient,
                             public quota::QuotaTaskObserver {
 public:
  CONTENT_EXPORT IndexedDBQuotaClient(
      base::MessageLoopProxy* tracker_thread,
      IndexedDBContext* indexed_db_context);
  CONTENT_EXPORT virtual ~IndexedDBQuotaClient();

  // QuotaClient method overrides
  virtual ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  virtual void GetOriginUsage(const GURL& origin_url,
                              quota::StorageType type,
                              GetUsageCallback* callback) OVERRIDE;
  virtual void GetOriginsForType(quota::StorageType type,
                                 GetOriginsCallback* callback) OVERRIDE;
  virtual void GetOriginsForHost(quota::StorageType type,
                                 const std::string& host,
                                 GetOriginsCallback* callback) OVERRIDE;
  virtual void DeleteOriginData(const GURL& origin,
                                quota::StorageType type,
                                DeletionCallback* callback) OVERRIDE;
 private:
  class HelperTask;
  class GetOriginUsageTask;
  class GetOriginsTaskBase;
  class GetAllOriginsTask;
  class GetOriginsForHostTask;
  class DeleteOriginTask;

  typedef quota::CallbackQueueMap1
      <GetUsageCallback*,
       GURL,  // origin
       int64
      > UsageForOriginCallbackMap;
  typedef quota::CallbackQueue2
      <GetOriginsCallback*,
       const std::set<GURL>&,
       quota::StorageType
      > OriginsForTypeCallbackQueue;
  typedef quota::CallbackQueueMap2
      <GetOriginsCallback*,
       std::string,  // host
       const std::set<GURL>&,
       quota::StorageType
      > OriginsForHostCallbackMap;

  void DidGetOriginUsage(const GURL& origin_url, int64 usage);
  void DidGetAllOrigins(const std::set<GURL>& origins, quota::StorageType type);
  void DidGetOriginsForHost(
      const std::string& host, const std::set<GURL>& origins,
          quota::StorageType type);

  scoped_refptr<base::MessageLoopProxy> webkit_thread_message_loop_;
  scoped_refptr<IndexedDBContext> indexed_db_context_;
  UsageForOriginCallbackMap usage_for_origin_callbacks_;
  OriginsForTypeCallbackQueue origins_for_type_callbacks_;
  OriginsForHostCallbackMap origins_for_host_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBQuotaClient);
};

#endif  // CONTENT_BROWER_IN_PROCESS_WEBKIT_QUOTA_CLIENT_H_
