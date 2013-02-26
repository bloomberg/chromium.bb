// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/resource_cache.h"

#include <string.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace policy {

ResourceCache::ResourceCache(const base::FilePath& cache_path) {
  // Allow the cache to be created in a different thread than the thread that's
  // going to use it.
  DetachFromThread();

  file_util::CreateDirectory(cache_path.DirName());
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::DB* db = NULL;
  leveldb::Status status =
      leveldb::DB::Open(options, cache_path.AsUTF8Unsafe(), &db);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to open leveldb at " << cache_path.AsUTF8Unsafe()
                 << ": " << status.ToString();
    // Maybe the database is busted; drop everything and try to create it again.
    file_util::Delete(cache_path, true);
    status = leveldb::DB::Open(options, cache_path.AsUTF8Unsafe(), &db);

    if (!status.ok())
      LOG(WARNING) << "Failed to open a new leveldb after wiping: "
                   << status.ToString();
  }
  db_.reset(db);
}

ResourceCache::~ResourceCache() {
  DCHECK(CalledOnValidThread());
}

bool ResourceCache::Store(const std::string& key,
                          const std::string& subkey,
                          const std::string& data) {
  DCHECK(CalledOnValidThread());
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return false;

  std::string path(CreatePath(key, subkey));
  return db_->Put(leveldb::WriteOptions(), path, data).ok();
}

bool ResourceCache::Load(const std::string& key,
                         const std::string& subkey,
                         std::string* data) {
  DCHECK(CalledOnValidThread());
  base::ThreadRestrictions::AssertIOAllowed();
  if (!IsOpen())
    return false;

  leveldb::ReadOptions options;
  options.fill_cache = false;
  std::string path(CreatePath(key, subkey));
  return db_->Get(options, path, data).ok();
}

void ResourceCache::LoadAllSubkeys(
    const std::string& key,
    std::map<std::string, std::string>* contents) {
  DCHECK(CalledOnValidThread());
  base::ThreadRestrictions::AssertIOAllowed();
  if (!IsOpen())
    return;

  contents->clear();

  const std::string prefix(CreatePathPrefix(key));
  leveldb::ReadOptions options;
  options.fill_cache = false;
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(options));
  for (it->Seek(prefix); it->Valid(); it->Next()) {
    if (!it->key().starts_with(prefix))
      break;
    const std::string subkey(GetSubkey(it->key().ToString()));
    leveldb::Slice slice = it->value();
    (*contents)[subkey].assign(slice.data(), slice.size());
  }
}

void ResourceCache::Delete(const std::string& key, const std::string& subkey) {
  DCHECK(CalledOnValidThread());
  base::ThreadRestrictions::AssertIOAllowed();
  if (!IsOpen())
    return;

  const std::string path(CreatePath(key, subkey));
  leveldb::Status status = db_->Delete(leveldb::WriteOptions(), path);
  if (!status.ok()) {
    LOG(WARNING) << "Failed to Delete \"" << path << "\" from leveldb: "
                 << status.ToString();
  }
}

void ResourceCache::PurgeOtherSubkeys(
    const std::string& key,
    const std::set<std::string>& subkeys_to_keep) {
  DCHECK(CalledOnValidThread());
  base::ThreadRestrictions::AssertIOAllowed();
  if (!IsOpen())
    return;

  leveldb::WriteBatch batch;
  const std::string prefix(CreatePathPrefix(key));
  leveldb::ReadOptions options;
  options.fill_cache = false;
  scoped_ptr<leveldb::Iterator> it(db_->NewIterator(options));
  for (it->Seek(prefix); it->Valid(); it->Next()) {
    if (!it->key().starts_with(prefix))
      break;
    const std::string subkey(GetSubkey(it->key().ToString()));
    if (subkeys_to_keep.find(subkey) == subkeys_to_keep.end())
      batch.Delete(it->key());
  }

  leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    LOG(WARNING) << "Purge of leveldb subkeys of " << key << " failed: "
                 << status.ToString();
  }
}

std::string ResourceCache::GetStringWithPrefix(const std::string& s) {
  char buffer[sizeof(size_t)];
  size_t size = s.size();
  memcpy(buffer, &size, sizeof(size_t));
  return std::string(buffer, sizeof(size_t)) + s;
}

std::string ResourceCache::CreatePathPrefix(const std::string& key) {
  return GetStringWithPrefix(key);
}

std::string ResourceCache::CreatePath(const std::string& key,
                                      const std::string& subkey) {
  return GetStringWithPrefix(key) + GetStringWithPrefix(subkey);
}

std::string ResourceCache::GetSubkey(const std::string& path) {
  // |path| was produced by CreatePath(). Skip the first key, then skip the
  // size of the subkey.
  if (path.size() < sizeof(size_t)) {
    NOTREACHED();
    return EmptyString();
  }

  // Skip the first string. |offset| is the start of the second string,
  // immediately preceded by its size.
  const size_t* size = reinterpret_cast<const size_t*>(path.data());
  size_t offset = sizeof(size_t) + *size + sizeof(size_t);
  if (path.size() < offset) {
    NOTREACHED();
    return EmptyString();
  }

  // Skip the size of the second string.
  size = reinterpret_cast<const size_t*>(path.data() + offset - sizeof(size_t));
  if (*size != path.size() - offset) {
    NOTREACHED();
    return EmptyString();
  }

  return path.substr(offset, *size);
}

}  // namespace policy
