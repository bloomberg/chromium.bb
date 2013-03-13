// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_SHADER_DISK_CACHE_H_
#define CONTENT_BROWSER_GPU_SHADER_DISK_CACHE_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "net/disk_cache/disk_cache.h"

namespace content {

class ShaderDiskCacheEntry;
class ShaderDiskReadHelper;

// ShaderDiskCache is the interface to the on disk cache for
// GL shaders.
//
// While this class is both RefCounted and SupportsWeakPtr
// when using this class you should work with the RefCounting.
// The WeakPtr is needed interally.
class ShaderDiskCache
    : public base::RefCounted<ShaderDiskCache>,
      public base::SupportsWeakPtr<ShaderDiskCache> {
 public:
  void Init();

  void set_host_id(int host_id) { host_id_ = host_id; }
  void set_max_cache_size(size_t max_cache_size) {
    max_cache_size_ = max_cache_size;
  }

  void Cache(const std::string& key, const std::string& shader);

 private:
  friend class base::RefCounted<ShaderDiskCache>;
  friend class ShaderDiskCacheEntry;
  friend class ShaderDiskReadHelper;
  friend class ShaderCacheFactory;

  explicit ShaderDiskCache(const base::FilePath& cache_path);
  ~ShaderDiskCache();

  void CacheCreatedCallback(int rv);

  disk_cache::Backend* backend() { return backend_; }

  void EntryComplete(void* entry);
  void ReadComplete();

  bool cache_available_;
  size_t max_cache_size_;
  int host_id_;
  base::FilePath cache_path_;
  bool is_initialized_;

  disk_cache::Backend* backend_;

  scoped_refptr<ShaderDiskReadHelper> helper_;
  std::map<void*, scoped_refptr<ShaderDiskCacheEntry> > entry_map_;

  DISALLOW_COPY_AND_ASSIGN(ShaderDiskCache);
};

// ShaderCacheFactory maintains a cache of ShaderDiskCache objects
// so we only create one per profile directory.
class ShaderCacheFactory {
 public:
  static ShaderCacheFactory* GetInstance();

  scoped_refptr<ShaderDiskCache> Get(int32 client_id);

  void SetCacheInfo(int32 client_id, const base::FilePath& path);
  void RemoveCacheInfo(int32 client_id);

  void AddToCache(const base::FilePath& path, ShaderDiskCache* cache);
  void RemoveFromCache(const base::FilePath& path);

 private:
  friend struct DefaultSingletonTraits<ShaderCacheFactory>;

  ShaderCacheFactory();
  ~ShaderCacheFactory();

  typedef std::map<base::FilePath, ShaderDiskCache*> ShaderCacheMap;
  ShaderCacheMap shader_cache_map_;

  typedef std::map<int32, base::FilePath> ClientIdToPathMap;
  ClientIdToPathMap client_id_to_path_map_;

  DISALLOW_COPY_AND_ASSIGN(ShaderCacheFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_SHADER_DISK_CACHE_H_

