// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_quota_client.h"

#include <vector>

#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "content/browser/in_process_webkit/indexed_db_context.h"
#include "net/base/net_util.h"
#include "webkit/database/database_util.h"

using quota::QuotaClient;


// Helper tasks ---------------------------------------------------------------

class IndexedDBQuotaClient::HelperTask : public quota::QuotaThreadTask {
 protected:
  HelperTask(
      IndexedDBQuotaClient* client,
      base::MessageLoopProxy* webkit_thread_message_loop)
      : QuotaThreadTask(client, webkit_thread_message_loop),
        client_(client), indexed_db_context_(client->indexed_db_context_) {
  }

  IndexedDBQuotaClient* client_;
  scoped_refptr<IndexedDBContext> indexed_db_context_;
};

class IndexedDBQuotaClient::GetOriginUsageTask : public HelperTask {
 public:
  GetOriginUsageTask(
      IndexedDBQuotaClient* client,
      base::MessageLoopProxy* webkit_thread_message_loop,
      const GURL& origin_url)
      : HelperTask(client, webkit_thread_message_loop),
        origin_url_(origin_url), usage_(0) {
  }

 private:
  virtual void RunOnTargetThread() OVERRIDE {
    string16 origin_id =
        webkit_database::DatabaseUtil::GetOriginIdentifier(origin_url_);
    FilePath file_path = indexed_db_context_->GetIndexedDBFilePath(origin_id);
    usage_ = 0;
    if (!file_util::GetFileSize(file_path, &usage_)) {
      LOG(ERROR) << "Failed to get file size for " << file_path.value();
    }
  }
  virtual void Completed() OVERRIDE {
    client_->DidGetOriginUsage(origin_url_, usage_);
  }
  GURL origin_url_;
  int64 usage_;
};

class IndexedDBQuotaClient::GetOriginsTaskBase : public HelperTask {
 protected:
  GetOriginsTaskBase(
      IndexedDBQuotaClient* client,
      base::MessageLoopProxy* webkit_thread_message_loop)
      : HelperTask(client, webkit_thread_message_loop) {
  }

  virtual bool ShouldAddOrigin(const GURL& origin) = 0;

  virtual void RunOnTargetThread() OVERRIDE {
    // TODO(dgrogan): Implement.
  }

  std::set<GURL> origins_;
};

class IndexedDBQuotaClient::GetAllOriginsTask : public GetOriginsTaskBase {
 public:
  GetAllOriginsTask(
      IndexedDBQuotaClient* client,
      base::MessageLoopProxy* webkit_thread_message_loop)
      : GetOriginsTaskBase(client, webkit_thread_message_loop) {
  }

 protected:
  virtual bool ShouldAddOrigin(const GURL& origin) OVERRIDE {
    return true;
  }
  virtual void Completed() OVERRIDE {
    client_->DidGetAllOrigins(origins_);
  }
};

class IndexedDBQuotaClient::GetOriginsForHostTask : public GetOriginsTaskBase {
 public:
  GetOriginsForHostTask(
      IndexedDBQuotaClient* client,
      base::MessageLoopProxy* webkit_thread_message_loop,
      const std::string& host)
      : GetOriginsTaskBase(client, webkit_thread_message_loop),
        host_(host) {
  }

 private:
  virtual bool ShouldAddOrigin(const GURL& origin) OVERRIDE {
    return host_ == net::GetHostOrSpecFromURL(origin);
  }
  virtual void Completed() OVERRIDE {
    client_->DidGetOriginsForHost(host_, origins_);
  }
  std::string host_;
};

// IndexedDBQuotaClient --------------------------------------------------------

IndexedDBQuotaClient::IndexedDBQuotaClient(
    base::MessageLoopProxy* webkit_thread_message_loop,
    IndexedDBContext* indexed_db_context)
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
    GetUsageCallback* callback_ptr) {
  DCHECK(callback_ptr);
  DCHECK(indexed_db_context_.get());
  scoped_ptr<GetUsageCallback> callback(callback_ptr);

  // IndexedDB is in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback->Run(0);
    return;
  }

  if (usage_for_origin_callbacks_.Add(origin_url, callback.release())) {
    scoped_refptr<GetOriginUsageTask> task(
        new GetOriginUsageTask(this, webkit_thread_message_loop_, origin_url));
    task->Start();
  }
}

void IndexedDBQuotaClient::GetOriginsForType(
    quota::StorageType type,
    GetOriginsCallback* callback_ptr) {
  DCHECK(callback_ptr);
  DCHECK(indexed_db_context_.get());
  scoped_ptr<GetOriginsCallback> callback(callback_ptr);

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback->Run(std::set<GURL>());
    return;
  }

  if (origins_for_type_callbacks_.Add(callback.release())) {
    scoped_refptr<GetAllOriginsTask> task(
        new GetAllOriginsTask(this, webkit_thread_message_loop_));
    task->Start();
  }
}

void IndexedDBQuotaClient::GetOriginsForHost(
    quota::StorageType type,
    const std::string& host,
    GetOriginsCallback* callback_ptr) {
  DCHECK(callback_ptr);
  DCHECK(indexed_db_context_.get());
  scoped_ptr<GetOriginsCallback> callback(callback_ptr);

  // All databases are in the temp namespace for now.
  if (type != quota::kStorageTypeTemporary) {
    callback->Run(std::set<GURL>());
    return;
  }

  if (origins_for_host_callbacks_.Add(host, callback.release())) {
    scoped_refptr<GetOriginsForHostTask> task(
        new GetOriginsForHostTask(this, webkit_thread_message_loop_, host));
    task->Start();
  }
}

void IndexedDBQuotaClient::DeleteOriginData(const GURL& origin,
                                           quota::StorageType type,
                                           DeletionCallback* callback) {
  // TODO(tzik): implement me
  callback->Run(quota::kQuotaErrorNotSupported);
  delete callback;
}

void IndexedDBQuotaClient::DidGetOriginUsage(
    const GURL& origin_url, int64 usage) {
  DCHECK(usage_for_origin_callbacks_.HasCallbacks(origin_url));
  usage_for_origin_callbacks_.Run(origin_url, usage);
}

void IndexedDBQuotaClient::DidGetAllOrigins(const std::set<GURL>& origins) {
  DCHECK(origins_for_type_callbacks_.HasCallbacks());
  origins_for_type_callbacks_.Run(origins);
}

void IndexedDBQuotaClient::DidGetOriginsForHost(
    const std::string& host, const std::set<GURL>& origins) {
  DCHECK(origins_for_host_callbacks_.HasCallbacks(host));
  origins_for_host_callbacks_.Run(host, origins);
}
