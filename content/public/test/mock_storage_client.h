// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_STORAGE_CLIENT_H_
#define CONTENT_PUBLIC_TEST_MOCK_STORAGE_CLIENT_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "webkit/browser/quota/quota_client.h"

namespace storage {
class QuotaManagerProxy;
}

using storage::QuotaClient;
using storage::QuotaManagerProxy;
using storage::StorageType;

namespace content {

struct MockOriginData {
  const char* origin;
  StorageType type;
  int64 usage;
};

// Mock storage class for testing.
class MockStorageClient : public QuotaClient {
 public:
  MockStorageClient(QuotaManagerProxy* quota_manager_proxy,
                    const MockOriginData* mock_data,
                    QuotaClient::ID id,
                    size_t mock_data_size);
  virtual ~MockStorageClient();

  // To add or modify mock data in this client.
  void AddOriginAndNotify(
      const GURL& origin_url, StorageType type, int64 size);
  void ModifyOriginAndNotify(
      const GURL& origin_url, StorageType type, int64 delta);
  void TouchAllOriginsAndNotify();

  void AddOriginToErrorSet(const GURL& origin_url, StorageType type);

  base::Time IncrementMockTime();

  // QuotaClient methods.
  virtual QuotaClient::ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  virtual void GetOriginUsage(const GURL& origin_url,
                              StorageType type,
                              const GetUsageCallback& callback) OVERRIDE;
  virtual void GetOriginsForType(StorageType type,
                                 const GetOriginsCallback& callback) OVERRIDE;
  virtual void GetOriginsForHost(StorageType type, const std::string& host,
                                 const GetOriginsCallback& callback) OVERRIDE;
  virtual void DeleteOriginData(const GURL& origin,
                                StorageType type,
                                const DeletionCallback& callback) OVERRIDE;
  virtual bool DoesSupport(storage::StorageType type) const OVERRIDE;

 private:
  void RunGetOriginUsage(const GURL& origin_url,
                         StorageType type,
                         const GetUsageCallback& callback);
  void RunGetOriginsForType(StorageType type,
                            const GetOriginsCallback& callback);
  void RunGetOriginsForHost(StorageType type,
                            const std::string& host,
                            const GetOriginsCallback& callback);
  void RunDeleteOriginData(const GURL& origin_url,
                           StorageType type,
                           const DeletionCallback& callback);

  void Populate(const MockOriginData* mock_data, size_t mock_data_size);

  scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  const ID id_;

  typedef std::map<std::pair<GURL, StorageType>, int64> OriginDataMap;
  OriginDataMap origin_data_;
  typedef std::set<std::pair<GURL, StorageType> > ErrorOriginSet;
  ErrorOriginSet error_origins_;

  int mock_time_counter_;

  base::WeakPtrFactory<MockStorageClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockStorageClient);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_STORAGE_CLIENT_H_
