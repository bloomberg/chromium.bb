// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_stores.h"

#include <string>

#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/service_worker_cache.pb.h"
#include "content/browser/service_worker/service_worker_fetch_store.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/directory_lister.h"

namespace content {

// Handles the loading of ServiceWorkerFetchStore and any extra clean up other
// than deleting the ServiceWorkerFetchStore object.
class ServiceWorkerFetchStores::StoresLoader {
 public:
  virtual ~StoresLoader() {};
  virtual ServiceWorkerFetchStore* LoadStore(const std::string& key) = 0;
  // Creates a new store, deleting any pre-existing store of the same name.
  virtual ServiceWorkerFetchStore* CreateStore(const std::string& key) = 0;
  virtual bool CleanUpDeletedStore(const std::string& key) = 0;
  virtual bool WriteIndex(StoreMap* stores) = 0;
  virtual void LoadIndex(std::vector<std::string>* names) = 0;
};

class ServiceWorkerFetchStores::MemoryLoader
    : public ServiceWorkerFetchStores::StoresLoader {
 public:
  virtual content::ServiceWorkerFetchStore* LoadStore(
      const std::string& key) OVERRIDE {
    NOTREACHED();
    return NULL;
  }

  virtual ServiceWorkerFetchStore* CreateStore(
      const std::string& key) OVERRIDE {
    return ServiceWorkerFetchStore::CreateMemoryStore(key);
  }

  virtual bool CleanUpDeletedStore(const std::string& key) OVERRIDE {
    return true;
  }

  virtual bool WriteIndex(StoreMap* stores) OVERRIDE { return false; }

  virtual void LoadIndex(std::vector<std::string>* names) OVERRIDE { return; }
};

class ServiceWorkerFetchStores::SimpleCacheLoader
    : public ServiceWorkerFetchStores::StoresLoader {
 public:
  explicit SimpleCacheLoader(const base::FilePath& origin_path)
      : origin_path_(origin_path) {}

  virtual ServiceWorkerFetchStore* LoadStore(const std::string& key) OVERRIDE {
    base::CreateDirectory(CreatePersistentStorePath(origin_path_, key));
    return ServiceWorkerFetchStore::CreatePersistentStore(
        CreatePersistentStorePath(origin_path_, key), key);
  }

  virtual ServiceWorkerFetchStore* CreateStore(
      const std::string& key) OVERRIDE {
    base::FilePath store_path = CreatePersistentStorePath(origin_path_, key);
    if (base::PathExists(store_path))
      base::DeleteFile(store_path, /* recursive */ true);
    return LoadStore(key);
  }

  virtual bool CleanUpDeletedStore(const std::string& key) OVERRIDE {
    return base::DeleteFile(CreatePersistentStorePath(origin_path_, key), true);
  }

  virtual bool WriteIndex(StoreMap* stores) OVERRIDE {
    ServiceWorkerCacheStorageIndex index;

    for (StoreMap::const_iterator iter(stores); !iter.IsAtEnd();
         iter.Advance()) {
      const ServiceWorkerFetchStore* store = iter.GetCurrentValue();
      ServiceWorkerCacheStorageIndex::Cache* cache = index.add_cache();
      cache->set_name(store->name());
      cache->set_size(0);  // TODO(jkarlin): Make this real.
    }

    std::string serialized;
    bool success = index.SerializeToString(&serialized);
    DCHECK(success);

    base::FilePath tmp_path = origin_path_.AppendASCII("index.txt.tmp");
    base::FilePath index_path = origin_path_.AppendASCII("index.txt");

    int bytes_written =
        base::WriteFile(tmp_path, serialized.c_str(), serialized.size());
    if (bytes_written != implicit_cast<int>(serialized.size())) {
      base::DeleteFile(tmp_path, /* recursive */ false);
      return false;
    }

    // Atomically rename the temporary index file to become the real one.
    return base::ReplaceFile(tmp_path, index_path, NULL);
  }

  virtual void LoadIndex(std::vector<std::string>* names) OVERRIDE {
    base::FilePath index_path = origin_path_.AppendASCII("index.txt");

    std::string serialized;
    if (!base::ReadFileToString(index_path, &serialized))
      return;

    ServiceWorkerCacheStorageIndex index;
    index.ParseFromString(serialized);

    for (int i = 0, max = index.cache_size(); i < max; ++i) {
      const ServiceWorkerCacheStorageIndex::Cache& cache = index.cache(i);
      names->push_back(cache.name());
    }

    // TODO(jkarlin): Delete stores that are in the directory and not returned
    // in LoadIndex.
    return;
  }

 private:
  std::string HexedHash(const std::string& value) {
    std::string value_hash = base::SHA1HashString(value);
    std::string valued_hexed_hash = base::StringToLowerASCII(
        base::HexEncode(value_hash.c_str(), value_hash.length()));
    return valued_hexed_hash;
  }

  base::FilePath CreatePersistentStorePath(const base::FilePath& origin_path,
                                           const std::string& store_name) {
    return origin_path.AppendASCII(HexedHash(store_name));
  }

  const base::FilePath origin_path_;
};

ServiceWorkerFetchStores::ServiceWorkerFetchStores(
    const base::FilePath& path,
    bool memory_only,
    const scoped_refptr<base::MessageLoopProxy>& callback_loop)
    : initialized_(false), origin_path_(path), callback_loop_(callback_loop) {
  if (memory_only)
    stores_loader_.reset(new MemoryLoader());
  else
    stores_loader_.reset(new SimpleCacheLoader(origin_path_));
}

ServiceWorkerFetchStores::~ServiceWorkerFetchStores() {
}

void ServiceWorkerFetchStores::CreateStore(
    const std::string& store_name,
    const StoreAndErrorCallback& callback) {
  LazyInit();

  if (store_name.empty()) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, FETCH_STORES_ERROR_EMPTY_KEY));
    return;
  }

  if (GetLoadedStore(store_name)) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, FETCH_STORES_ERROR_EXISTS));
    return;
  }

  ServiceWorkerFetchStore* store = stores_loader_->CreateStore(store_name);

  if (!store) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, FETCH_STORES_ERROR_STORAGE));
    return;
  }

  InitStore(store);

  stores_loader_->WriteIndex(&store_map_);

  store->CreateBackend(
      base::Bind(&ServiceWorkerFetchStores::InitializeStoreCallback,
                 base::Unretained(this),
                 store,
                 callback));
}

void ServiceWorkerFetchStores::GetStore(const std::string& store_name,
                                        const StoreAndErrorCallback& callback) {
  LazyInit();

  if (store_name.empty()) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, FETCH_STORES_ERROR_EMPTY_KEY));
    return;
  }

  ServiceWorkerFetchStore* store = GetLoadedStore(store_name);
  if (!store) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, FETCH_STORES_ERROR_NOT_FOUND));
    return;
  }

  store->CreateBackend(
      base::Bind(&ServiceWorkerFetchStores::InitializeStoreCallback,
                 base::Unretained(this),
                 store,
                 callback));
}

void ServiceWorkerFetchStores::HasStore(const std::string& store_name,
                                        const BoolAndErrorCallback& callback) {
  LazyInit();

  if (store_name.empty()) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, false, FETCH_STORES_ERROR_EMPTY_KEY));
    return;
  }

  bool has_store = GetLoadedStore(store_name) != NULL;

  callback_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          callback, has_store, FETCH_STORES_ERROR_NO_ERROR));
}

void ServiceWorkerFetchStores::DeleteStore(
    const std::string& store_name,
    const BoolAndErrorCallback& callback) {
  LazyInit();

  if (store_name.empty()) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, false, FETCH_STORES_ERROR_EMPTY_KEY));
    return;
  }

  ServiceWorkerFetchStore* store = GetLoadedStore(store_name);
  if (!store) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, false, FETCH_STORES_ERROR_NOT_FOUND));
    return;
  }

  name_map_.erase(store_name);
  store_map_.Remove(store->id());  // deletes store

  stores_loader_->WriteIndex(&store_map_);  // Update the index.

  stores_loader_->CleanUpDeletedStore(store_name);

  callback_loop_->PostTask(
      FROM_HERE, base::Bind(callback, true, FETCH_STORES_ERROR_NO_ERROR));
}

void ServiceWorkerFetchStores::EnumerateStores(
    const StringsAndErrorCallback& callback) {
  LazyInit();

  std::vector<std::string> names;
  for (NameMap::const_iterator it = name_map_.begin(); it != name_map_.end();
       ++it) {
    names.push_back(it->first);
  }

  callback_loop_->PostTask(
      FROM_HERE, base::Bind(callback, names, FETCH_STORES_ERROR_NO_ERROR));
}

void ServiceWorkerFetchStores::InitializeStoreCallback(
    const ServiceWorkerFetchStore* store,
    const StoreAndErrorCallback& callback,
    bool success) {
  if (!success) {
    // TODO(jkarlin): This should delete the directory and try again in case
    // the cache is simply corrupt.
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, FETCH_STORES_ERROR_STORAGE));
    return;
  }
  callback_loop_->PostTask(
      FROM_HERE,
      base::Bind(callback, store->id(), FETCH_STORES_ERROR_NO_ERROR));
}

// Init is run lazily so that it is called on the proper MessageLoop.
void ServiceWorkerFetchStores::LazyInit() {
  if (initialized_)
    return;

  std::vector<std::string> indexed_store_names;
  stores_loader_->LoadIndex(&indexed_store_names);

  for (std::vector<std::string>::const_iterator it =
           indexed_store_names.begin();
       it != indexed_store_names.end();
       ++it) {
    ServiceWorkerFetchStore* store = stores_loader_->LoadStore(*it);
    InitStore(store);
  }
  initialized_ = true;
}

void ServiceWorkerFetchStores::InitStore(ServiceWorkerFetchStore* store) {
  StoreID id = store_map_.Add(store);
  name_map_.insert(std::make_pair(store->name(), id));
  store->set_id(id);
}

ServiceWorkerFetchStore* ServiceWorkerFetchStores::GetLoadedStore(
    const std::string& key) const {
  DCHECK(initialized_);

  NameMap::const_iterator it = name_map_.find(key);
  if (it == name_map_.end())
    return NULL;

  ServiceWorkerFetchStore* store = store_map_.Lookup(it->second);
  DCHECK(store);
  return store;
}

}  // namespace content
