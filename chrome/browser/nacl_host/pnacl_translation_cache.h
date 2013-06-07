// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_PNACL_TRANSLATION_CACHE_H_
#define CHROME_BROWSER_NACL_HOST_PNACL_TRANSLATION_CACHE_H_

#include <map>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "net/base/cache_type.h"

namespace base {
class MessageLoopProxy;
}

namespace disk_cache {
class Backend;
}

namespace pnacl_cache {
typedef base::Callback<void(int)> CompletionCallback;
class PNaClTranslationCacheEntry;
extern const int kMaxMemCacheSize;

class PNaClTranslationCache
    : public base::SupportsWeakPtr<PNaClTranslationCache> {
 public:
  PNaClTranslationCache();
  virtual ~PNaClTranslationCache();

  // Initialize the translation cache in |cache_dir| (or in memory if
  // |in_memory| is true). Call |callback| with a 0 argument on sucess and
  // <0 otherwise.
  int InitCache(const base::FilePath& cache_dir,
                bool in_memory,
                const CompletionCallback& callback);

  // Store the nexe in the translation cache.
  void StoreNexe(const std::string& key, const std::string& nexe);

  // Store the nexe in the translation cache, and call |callback| with
  // the result. The result passed to the callback is 0 on success and
  // <0 otherwise.
  void StoreNexe(const std::string& key,
                 const std::string& nexe,
                 const CompletionCallback& callback);

  // Retrieve the nexe from the translation cache. Write the data into |nexe|
  // and call |callback| with the result (0 on success and <0 otherwise)
  void GetNexe(const std::string& key,
               std::string* nexe,
               const CompletionCallback& callback);

  // Return the number of entries in the cache backend.
  int Size();

 private:
  friend class PNaClTranslationCacheEntry;
  // PNaClTranslationCacheEntry should only use the
  // OpComplete and backend methods on PNaClTranslationCache.
  void OpComplete(PNaClTranslationCacheEntry* entry);
  disk_cache::Backend* backend() { return disk_cache_; }

  int InitWithDiskBackend(const base::FilePath& disk_cache_dir,
                          int cache_size,
                          const CompletionCallback& callback);

  int InitWithMemBackend(int cache_size, const CompletionCallback& callback);

  int Init(net::CacheType,
           const base::FilePath& directory,
           int cache_size,
           const CompletionCallback& callback);

  void OnCreateBackendComplete(int rv);

  disk_cache::Backend* disk_cache_;
  CompletionCallback init_callback_;
  bool in_memory_;
  std::map<void*, scoped_refptr<PNaClTranslationCacheEntry> > open_entries_;

  DISALLOW_COPY_AND_ASSIGN(PNaClTranslationCache);
};

}  // namespace pnacl_cache

#endif  // CHROME_BROWSER_NACL_HOST_PNACL_TRANSLATION_CACHE_H_
