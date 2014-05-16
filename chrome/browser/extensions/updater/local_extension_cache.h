// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_LOCAL_EXTENSION_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_LOCAL_EXTENSION_CACHE_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace extensions {

// Cache .crx files in some local dir for future use. Cache keeps only latest
// version of the extensions. Only one instance of LocalExtensionCache can work
// with the same directory. But LocalExtensionCache instance can be shared
// between multiple clients. Public interface can be used only from UI thread.
class LocalExtensionCache {
 public:
  // Callback invoked on UI thread when PutExtension is completed.
  typedef base::Callback<void(const base::FilePath& file_path,
                              bool file_ownership_passed)> PutExtensionCallback;

  // |cache_dir| - directory that will be used for caching CRX files.
  // |max_cache_size| - maximum disk space that cache can use, 0 means no limit.
  // |max_cache_age| - maximum age that unused item can be kept in cache, 0 age
  // means that all unused cache items will be removed on Shutdown.
  // All file I/O is done via the |backend_task_runner|.
  LocalExtensionCache(const base::FilePath& cache_dir,
                      uint64 max_cache_size,
                      const base::TimeDelta& max_cache_age,
                      const scoped_refptr<base::SequencedTaskRunner>&
                          backend_task_runner);
  ~LocalExtensionCache();

  // Name of flag file that indicates that cache is ready (import finished).
  static const char kCacheReadyFlagFileName[];

  // Initialize cache. If |wait_for_cache_initialization| is |true|, the cache
  // contents will not be read until a flag file appears in the cache directory,
  // signaling that the cache is ready. The |callback| is called when cache is
  // ready and cache dir content was already checked.
  void Init(bool wait_for_cache_initialization,
            const base::Closure& callback);

  // Shut down the cache. The |callback| will be invoked when the cache has shut
  // down completely and there are no more pending file I/O operations.
  void Shutdown(const base::Closure& callback);

  // If extension with |id| exists in the cache, returns |true|, |file_path| and
  // |version| for the extension. Extension will be marked as used with current
  // timestamp.
  bool GetExtension(const std::string& id,
                    base::FilePath* file_path,
                    std::string* version);

  // Put extension with |id| and |version| into local cache. Older version in
  // the cache will be on next run so it can be safely used. Extension will be
  // marked as used with current timestamp. The file will be available via
  // GetExtension when |callback| is called. PutExtension may get ownership
  // of |file_path| or return it back via |callback|.
  void PutExtension(const std::string& id,
                    const base::FilePath& file_path,
                    const std::string& version,
                    const PutExtensionCallback& callback);

  // Remove extension with |id| from local cache, corresponding crx file will be
  // removed from disk too.
  bool RemoveExtension(const std::string& id);

  // Return cache statistics. Returns |false| if cache is not ready.
  bool GetStatistics(uint64* cache_size,
                     size_t* extensions_count);

  bool is_ready() const { return state_ == kReady; }
  bool is_uninitialized() const { return state_ == kUninitialized; }
  bool is_shutdown() const { return state_ == kShutdown; }

  // For tests only!
  void SetCacheStatusPollingDelayForTests(const base::TimeDelta& delay);

 private:
  struct CacheItemInfo {
    std::string version;
    base::Time last_used;
    uint64 size;
    base::FilePath file_path;

    CacheItemInfo(const std::string& version,
                  const base::Time& last_used,
                  uint64 size,
                  const base::FilePath& file_path);
  };
  typedef std::map<std::string, CacheItemInfo> CacheMap;

  enum State {
    kUninitialized,
    kWaitInitialization,
    kReady,
    kShutdown
  };

  // Sends BackendCheckCacheStatus task on backend thread.
  void CheckCacheStatus(const base::Closure& callback);

  // Checks whether a flag file exists in the |cache_dir|, indicating that the
  // cache is ready. This method is invoked via the |backend_task_runner_| and
  // posts its result back to the |local_cache| on the UI thread.
  static void BackendCheckCacheStatus(
      base::WeakPtr<LocalExtensionCache> local_cache,
      const base::FilePath& cache_dir,
      const base::Closure& callback);

  // Invoked on the UI thread after checking whether the cache is ready. If the
  // cache is not ready yet, posts a delayed task that will repeat the check,
  // thus polling for cache readiness.
  void OnCacheStatusChecked(bool ready, const base::Closure& callback);

  // Checks the cache contents. This is a helper that invokes the actual check
  // by posting to the |backend_task_runner_|.
  void CheckCacheContents(const base::Closure& callback);

  // Checks the cache contents. This method is invoked via the
  // |backend_task_runner_| and posts back a list of cache entries to the
  // |local_cache| on the UI thread.
  static void BackendCheckCacheContents(
      base::WeakPtr<LocalExtensionCache> local_cache,
      const base::FilePath& cache_dir,
      const base::Closure& callback);

  // Helper for BackendCheckCacheContents() that updates |cache_content|.
  static void BackendCheckCacheContentsInternal(
      const base::FilePath& cache_dir,
      CacheMap* cache_content);

  // Invoked when the cache content on disk has been checked. |cache_content|
  // contains all the currently valid crx files in the cache.
  void OnCacheContentsChecked(scoped_ptr<CacheMap> cache_content,
                              const base::Closure& callback);

  // Update timestamp for the file to mark it as "used". This method is invoked
  // via the |backend_task_runner_|.
  static void BackendMarkFileUsed(const base::FilePath& file_path,
                                  const base::Time& time);

  // Installs the downloaded crx file at |path| in the |cache_dir|. This method
  // is invoked via the |backend_task_runner_|.
  static void BackendInstallCacheEntry(
      base::WeakPtr<LocalExtensionCache> local_cache,
      const base::FilePath& cache_dir,
      const std::string& id,
      const base::FilePath& file_path,
      const std::string& version,
      const PutExtensionCallback& callback);

  // Invoked on the UI thread when a new entry has been installed in the cache.
  void OnCacheEntryInstalled(const std::string& id,
                             const CacheItemInfo& info,
                             bool was_error,
                             const PutExtensionCallback& callback);

  // Remove cached crx files(all versions) under |cached_dir| for extension with
  // |id|. This method is invoked via the |backend_task_runner_|.
  static void BackendRemoveCacheEntry(const base::FilePath& cache_dir,
                                      const std::string& id);

  // Compare two cache items returns true if first item is older.
  static bool CompareCacheItemsAge(const CacheMap::iterator& lhs,
                                   const CacheMap::iterator& rhs);

  // Calculate which files need to be deleted and schedule files deletion.
  void CleanUp();

  // Path to the directory where the extension cache is stored.
  base::FilePath cache_dir_;

  // Maximum size of cache dir on disk.
  uint64 max_cache_size_;

  // Minimal age of unused item in cache, items prior to this age will be
  // deleted on shutdown.
  base::Time min_cache_age_;

  // Task runner for executing file I/O tasks.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Track state of the instance.
  State state_;

  // This contains info about all cached extensions.
  CacheMap cached_extensions_;

  // Weak factory for callbacks from the backend and delayed tasks.
  base::WeakPtrFactory<LocalExtensionCache> weak_ptr_factory_;

  // Delay between polling cache status.
  base::TimeDelta cache_status_polling_delay_;

  DISALLOW_COPY_AND_ASSIGN(LocalExtensionCache);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_LOCAL_EXTENSION_CACHE_H_
