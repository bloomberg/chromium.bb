// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/v4_test_utils.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "crypto/sha2.h"

namespace safe_browsing {

TestV4Store::TestV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path)
    : V4Store(task_runner, store_path) {}

TestV4Store::~TestV4Store() = default;

bool TestV4Store::HasValidData() const {
  return true;
}

void TestV4Store::MarkPrefixAsBad(HashPrefix prefix) {
  hash_prefix_map_[prefix.size()] = prefix;
}

TestV4Database::TestV4Database(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map)
    : V4Database(db_task_runner, std::move(store_map)) {}

void TestV4Database::MarkPrefixAsBad(ListIdentifier list_id,
                                     HashPrefix prefix) {
  V4Store* base_store = store_map_->at(list_id).get();
  TestV4Store* test_store = static_cast<TestV4Store*>(base_store);
  test_store->MarkPrefixAsBad(prefix);
}

TestV4StoreFactory::TestV4StoreFactory() = default;

TestV4StoreFactory::~TestV4StoreFactory() = default;

std::unique_ptr<V4Store> TestV4StoreFactory::CreateV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path) {
  auto new_store = base::MakeUnique<TestV4Store>(task_runner, store_path);
  new_store->Initialize();
  return std::move(new_store);
}

TestV4DatabaseFactory::TestV4DatabaseFactory() = default;

TestV4DatabaseFactory::~TestV4DatabaseFactory() = default;

std::unique_ptr<V4Database> TestV4DatabaseFactory::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map) {
  auto v4_db =
      base::MakeUnique<TestV4Database>(db_task_runner, std::move(store_map));
  v4_db_ = v4_db.get();
  return std::move(v4_db);
}

void TestV4DatabaseFactory::MarkPrefixAsBad(ListIdentifier list_id,
                                            HashPrefix prefix) {
  v4_db_->MarkPrefixAsBad(list_id, prefix);
}

TestV4GetHashProtocolManager::TestV4GetHashProtocolManager(
    net::URLRequestContextGetter* request_context_getter,
    const StoresToCheck& stores_to_check,
    const V4ProtocolConfig& config)
    : V4GetHashProtocolManager(request_context_getter,
                               stores_to_check,
                               config) {}

void TestV4GetHashProtocolManager::AddToFullHashCache(FullHashInfo fhi) {
  full_hash_cache_[fhi.full_hash].full_hash_infos.push_back(fhi);
}

TestV4GetHashProtocolManagerFactory::TestV4GetHashProtocolManagerFactory() =
    default;

TestV4GetHashProtocolManagerFactory::~TestV4GetHashProtocolManagerFactory() =
    default;

std::unique_ptr<V4GetHashProtocolManager>
TestV4GetHashProtocolManagerFactory::CreateProtocolManager(
    net::URLRequestContextGetter* request_context_getter,
    const StoresToCheck& stores_to_check,
    const V4ProtocolConfig& config) {
  auto pm = base::MakeUnique<TestV4GetHashProtocolManager>(
      request_context_getter, stores_to_check, config);
  pm_ = pm.get();
  return std::move(pm);
}

FullHash GetFullHash(const GURL& url) {
  std::string host;
  std::string path;
  V4ProtocolManagerUtil::CanonicalizeUrl(url, &host, &path, nullptr);

  return crypto::SHA256HashString(host + path);
}

FullHashInfo GetFullHashInfo(const GURL& url, const ListIdentifier& list_id) {
  return FullHashInfo(GetFullHash(url), list_id,
                      base::Time::Now() + base::TimeDelta::FromMinutes(5));
}

FullHashInfo GetFullHashInfoWithMetadata(
    const GURL& url,
    const ListIdentifier& list_id,
    const ThreatMetadata& threat_metadata) {
  FullHashInfo fhi = GetFullHashInfo(url, list_id);
  fhi.metadata = threat_metadata;
  return fhi;
}

}  // namespace safe_browsing
