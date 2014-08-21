// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/local_extension_cache.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/version.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace {

// File name extension for CRX files (not case sensitive).
const char kCRXFileExtension[] = ".crx";

// Delay between checks for flag file presence when waiting for the cache to
// become ready.
const int64_t kCacheStatusPollingDelayMs = 1000;

}  // namespace

const char LocalExtensionCache::kCacheReadyFlagFileName[] = ".initialized";

LocalExtensionCache::LocalExtensionCache(
    const base::FilePath& cache_dir,
    uint64 max_cache_size,
    const base::TimeDelta& max_cache_age,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner)
    : cache_dir_(cache_dir),
      max_cache_size_(max_cache_size),
      min_cache_age_(base::Time::Now() - max_cache_age),
      backend_task_runner_(backend_task_runner),
      state_(kUninitialized),
      weak_ptr_factory_(this),
      cache_status_polling_delay_(
          base::TimeDelta::FromMilliseconds(kCacheStatusPollingDelayMs)) {
}

LocalExtensionCache::~LocalExtensionCache() {
  if (state_ == kReady)
    CleanUp();
}

void LocalExtensionCache::Init(bool wait_for_cache_initialization,
                               const base::Closure& callback) {
  DCHECK_EQ(state_, kUninitialized);

  state_ = kWaitInitialization;
  if (wait_for_cache_initialization)
    CheckCacheStatus(callback);
  else
    CheckCacheContents(callback);
}

void LocalExtensionCache::Shutdown(const base::Closure& callback) {
  DCHECK_NE(state_, kShutdown);
  if (state_ == kReady)
    CleanUp();
  state_ = kShutdown;
  backend_task_runner_->PostTaskAndReply(FROM_HERE,
      base::Bind(&base::DoNothing), callback);
}

bool LocalExtensionCache::GetExtension(const std::string& id,
                                       base::FilePath* file_path,
                                       std::string* version) {
  if (state_ != kReady)
    return false;

  CacheMap::iterator it = cached_extensions_.find(id);
  if (it == cached_extensions_.end())
    return false;

  if (file_path) {
    *file_path = it->second.file_path;

    // If caller is not interesting in file_path, extension is not used.
    base::Time now = base::Time::Now();
    backend_task_runner_->PostTask(FROM_HERE,
        base::Bind(&LocalExtensionCache::BackendMarkFileUsed,
        it->second.file_path, now));
    it->second.last_used = now;
  }

  if (version)
    *version = it->second.version;

  return true;
}

void LocalExtensionCache::PutExtension(const std::string& id,
                                       const base::FilePath& file_path,
                                       const std::string& version,
                                       const PutExtensionCallback& callback) {
  if (state_ != kReady) {
    callback.Run(file_path, true);
    return;
  }

  Version version_validator(version);
  if (!version_validator.IsValid()) {
    LOG(ERROR) << "Extension " << id << " has bad version " << version;
    callback.Run(file_path, true);
    return;
  }

  CacheMap::iterator it = cached_extensions_.find(id);
  if (it != cached_extensions_.end()) {
    Version new_version(version);
    Version prev_version(it->second.version);
    if (new_version.CompareTo(prev_version) <= 0) {
      LOG(WARNING) << "Cache contains newer or the same version "
                   << prev_version.GetString() << " for extension "
                   << id << " version " << version;
      callback.Run(file_path, true);
      return;
    }
  }

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalExtensionCache::BackendInstallCacheEntry,
                  weak_ptr_factory_.GetWeakPtr(),
                  cache_dir_,
                  id,
                  file_path,
                  version,
                  callback));
}

bool LocalExtensionCache::RemoveExtension(const std::string& id) {
  if (state_ != kReady)
    return false;

  CacheMap::iterator it = cached_extensions_.find(id);
  if (it == cached_extensions_.end())
    return false;

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &LocalExtensionCache::BackendRemoveCacheEntry, cache_dir_, id));

  cached_extensions_.erase(it);
  return true;
}

bool LocalExtensionCache::GetStatistics(uint64* cache_size,
                                        size_t* extensions_count) {
  if (state_ != kReady)
    return false;

  *cache_size = 0;
  for (CacheMap::iterator it = cached_extensions_.begin();
       it != cached_extensions_.end(); ++it) {
    *cache_size += it->second.size;
  }
  *extensions_count = cached_extensions_.size();

  return true;
}

void LocalExtensionCache::SetCacheStatusPollingDelayForTests(
    const base::TimeDelta& delay) {
  cache_status_polling_delay_ = delay;
}

void LocalExtensionCache::CheckCacheStatus(const base::Closure& callback) {
  if (state_ == kShutdown) {
    callback.Run();
    return;
  }

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalExtensionCache::BackendCheckCacheStatus,
                  weak_ptr_factory_.GetWeakPtr(),
                  cache_dir_,
                  callback));
}

// static
void LocalExtensionCache::BackendCheckCacheStatus(
    base::WeakPtr<LocalExtensionCache> local_cache,
    const base::FilePath& cache_dir,
    const base::Closure& callback) {
  const bool exists =
      base::PathExists(cache_dir.AppendASCII(kCacheReadyFlagFileName));

  static bool first_check = true;
  if (first_check && !exists && !base::SysInfo::IsRunningOnChromeOS()) {
    LOG(WARNING) << "Extensions will not be installed from update URLs until "
                 << cache_dir.AppendASCII(kCacheReadyFlagFileName).value()
                 << " exists.";
  }
  first_check = false;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&LocalExtensionCache::OnCacheStatusChecked,
                 local_cache,
                 exists,
                 callback));
}

void LocalExtensionCache::OnCacheStatusChecked(bool ready,
                                               const base::Closure& callback) {
  if (state_ == kShutdown) {
    callback.Run();
    return;
  }

  if (ready) {
    CheckCacheContents(callback);
  } else {
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&LocalExtensionCache::CheckCacheStatus,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback),
        cache_status_polling_delay_);
  }
}

void LocalExtensionCache::CheckCacheContents(const base::Closure& callback) {
  DCHECK_EQ(state_, kWaitInitialization);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalExtensionCache::BackendCheckCacheContents,
                 weak_ptr_factory_.GetWeakPtr(),
                 cache_dir_,
                 callback));
}

// static
void LocalExtensionCache::BackendCheckCacheContents(
    base::WeakPtr<LocalExtensionCache> local_cache,
    const base::FilePath& cache_dir,
    const base::Closure& callback) {
  scoped_ptr<CacheMap> cache_content(new CacheMap);
  BackendCheckCacheContentsInternal(cache_dir, cache_content.get());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&LocalExtensionCache::OnCacheContentsChecked,
                 local_cache,
                 base::Passed(&cache_content),
                 callback));
}

// static
void LocalExtensionCache::BackendCheckCacheContentsInternal(
    const base::FilePath& cache_dir,
    CacheMap* cache_content) {
  // Start by verifying that the cache_dir exists.
  if (!base::DirectoryExists(cache_dir)) {
    // Create it now.
    if (!base::CreateDirectory(cache_dir)) {
      LOG(ERROR) << "Failed to create cache directory at "
                 << cache_dir.value();
    }

    // Nothing else to do. Cache is empty.
    return;
  }

  // Enumerate all the files in the cache |cache_dir|, including directories
  // and symlinks. Each unrecognized file will be erased.
  int types = base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES;
  base::FileEnumerator enumerator(cache_dir, false /* recursive */, types);
  for (base::FilePath path = enumerator.Next();
       !path.empty(); path = enumerator.Next()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    std::string basename = path.BaseName().value();

    if (info.IsDirectory() || base::IsLink(info.GetName())) {
      LOG(ERROR) << "Erasing bad file in cache directory: " << basename;
      base::DeleteFile(path, true /* recursive */);
      continue;
    }

    // Skip flag file that indicates that cache is ready.
    if (basename == kCacheReadyFlagFileName)
      continue;

    // crx files in the cache are named <extension-id>-<version>.crx.
    std::string id;
    std::string version;
    if (EndsWith(basename, kCRXFileExtension, false /* case-sensitive */)) {
      size_t n = basename.find('-');
      if (n != std::string::npos && n + 1 < basename.size() - 4) {
        id = basename.substr(0, n);
        // Size of |version| = total size - "<id>" - "-" - ".crx"
        version = basename.substr(n + 1, basename.size() - 5 - id.size());
      }
    }

    // Enforce a lower-case id.
    id = base::StringToLowerASCII(id);
    if (!crx_file::id_util::IdIsValid(id)) {
      LOG(ERROR) << "Bad extension id in cache: " << id;
      id.clear();
    }

    if (!Version(version).IsValid()) {
      LOG(ERROR) << "Bad extension version in cache: " << version;
      version.clear();
    }

    if (id.empty() || version.empty()) {
      LOG(ERROR) << "Invalid file in cache, erasing: " << basename;
      base::DeleteFile(path, true /* recursive */);
      continue;
    }

    VLOG(1) << "Found cached version " << version
            << " for extension id " << id;

    CacheMap::iterator it = cache_content->find(id);
    if (it != cache_content->end()) {
      // |cache_content| already has version for this ID. Removed older one.
      Version curr_version(version);
      Version prev_version(it->second.version);
      if (prev_version.CompareTo(curr_version) <= 0) {
        base::DeleteFile(base::FilePath(it->second.file_path),
                         true /* recursive */);
        cache_content->erase(id);
        VLOG(1) << "Remove older version " << it->second.version
                << " for extension id " << id;
      } else {
        base::DeleteFile(path, true /* recursive */);
        VLOG(1) << "Remove older version " << version
                << " for extension id " << id;
        continue;
      }
    }

    cache_content->insert(std::make_pair(id, CacheItemInfo(
        version, info.GetLastModifiedTime(), info.GetSize(), path)));
  }
}

void LocalExtensionCache::OnCacheContentsChecked(
    scoped_ptr<CacheMap> cache_content,
    const base::Closure& callback) {
  cache_content->swap(cached_extensions_);
  state_ = kReady;
  callback.Run();
}

// static
void LocalExtensionCache::BackendMarkFileUsed(const base::FilePath& file_path,
                                              const base::Time& time) {
  base::TouchFile(file_path, time, time);
}

// static
void LocalExtensionCache::BackendInstallCacheEntry(
    base::WeakPtr<LocalExtensionCache> local_cache,
    const base::FilePath& cache_dir,
    const std::string& id,
    const base::FilePath& file_path,
    const std::string& version,
    const PutExtensionCallback& callback) {
  std::string basename = id + "-" + version + kCRXFileExtension;
  base::FilePath cached_crx_path = cache_dir.AppendASCII(basename);

  bool was_error = false;
  if (base::PathExists(cached_crx_path)) {
    LOG(ERROR) << "File already exists " << file_path.value();
    cached_crx_path = file_path;
    was_error = true;
  }

  base::File::Info info;
  if (!was_error) {
    if (!base::Move(file_path, cached_crx_path)) {
      LOG(ERROR) << "Failed to copy from " << file_path.value()
                 << " to " << cached_crx_path.value();
      cached_crx_path = file_path;
      was_error = true;
    } else {
      was_error = !base::GetFileInfo(cached_crx_path, &info);
      VLOG(1) << "Cache entry installed for extension id " << id
              << " version " << version;
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&LocalExtensionCache::OnCacheEntryInstalled,
                 local_cache,
                 id,
                 CacheItemInfo(version, info.last_modified,
                               info.size, cached_crx_path),
                 was_error,
                 callback));
}

void LocalExtensionCache::OnCacheEntryInstalled(
    const std::string& id,
    const CacheItemInfo& info,
    bool was_error,
    const PutExtensionCallback& callback) {
  if (state_ == kShutdown || was_error) {
    callback.Run(info.file_path, true);
    return;
  }

  CacheMap::iterator it = cached_extensions_.find(id);
  if (it != cached_extensions_.end()) {
    Version new_version(info.version);
    Version prev_version(it->second.version);
    if (new_version.CompareTo(prev_version) <= 0) {
      DCHECK(0) << "Cache contains newer or the same version";
      callback.Run(info.file_path, true);
      return;
    }
    it->second = info;
  } else {
    it = cached_extensions_.insert(std::make_pair(id, info)).first;
  }
  // Time from file system can have lower precision so use precise "now".
  it->second.last_used = base::Time::Now();

  callback.Run(info.file_path, false);
}

// static
void LocalExtensionCache::BackendRemoveCacheEntry(
    const base::FilePath& cache_dir,
    const std::string& id) {
  std::string file_pattern = id + "-*" + kCRXFileExtension;
  base::FileEnumerator enumerator(cache_dir,
                                  false /* not recursive */,
                                  base::FileEnumerator::FILES,
                                  file_pattern);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    base::DeleteFile(path, false);
    VLOG(1) << "Removed cached file " << path.value();
  }
}

// static
bool LocalExtensionCache::CompareCacheItemsAge(const CacheMap::iterator& lhs,
                                               const CacheMap::iterator& rhs) {
  return lhs->second.last_used < rhs->second.last_used;
}

void LocalExtensionCache::CleanUp() {
  DCHECK_EQ(state_, kReady);

  std::vector<CacheMap::iterator> items;
  items.reserve(cached_extensions_.size());
  uint64_t total_size = 0;
  for (CacheMap::iterator it = cached_extensions_.begin();
       it != cached_extensions_.end(); ++it) {
    items.push_back(it);
    total_size += it->second.size;
  }
  std::sort(items.begin(), items.end(), CompareCacheItemsAge);

  for (std::vector<CacheMap::iterator>::iterator it = items.begin();
       it != items.end(); ++it) {
    if ((*it)->second.last_used < min_cache_age_ ||
        (max_cache_size_ && total_size > max_cache_size_)) {
      total_size -= (*it)->second.size;
      VLOG(1) << "Clean up cached extension id " << (*it)->first;
      RemoveExtension((*it)->first);
    }
  }
}

LocalExtensionCache::CacheItemInfo::CacheItemInfo(
    const std::string& version,
    const base::Time& last_used,
    uint64 size,
    const base::FilePath& file_path)
    : version(version), last_used(last_used), size(size), file_path(file_path) {
}

}  // namespace extensions
