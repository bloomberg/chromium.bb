// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_cache_impl.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace extensions {
namespace {

#if defined(OS_CHROMEOS)
const char kLocalCacheDir[] = "/var/cache/external_cache";
#else
#error Please define kLocalCacheDir suitable for target OS
#endif// Directory where the extensions are cached.

// Maximum size of local cache on disk.
size_t kMaxCacheSize = 100 * 1024 * 1024;

// Maximum age of unused extensions in cache.
const int kMaxCacheAgeDays = 30;

}  // namespace

// static
ExtensionCacheImpl* ExtensionCacheImpl::GetInstance() {
  return Singleton<ExtensionCacheImpl>::get();
}

ExtensionCacheImpl::ExtensionCacheImpl()
  : cache_(new LocalExtensionCache(base::FilePath(kLocalCacheDir),
        kMaxCacheSize,
        base::TimeDelta::FromDays(kMaxCacheAgeDays),
        content::BrowserThread::GetBlockingPool()->
            GetSequencedTaskRunnerWithShutdownBehavior(
                content::BrowserThread::GetBlockingPool()->GetSequenceToken(),
                base::SequencedWorkerPool::SKIP_ON_SHUTDOWN))),
    weak_ptr_factory_(this) {
  notification_registrar_.Add(
      this,
      extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllBrowserContextsAndSources());
  cache_->Init(true, base::Bind(&ExtensionCacheImpl::OnCacheInitialized,
                                weak_ptr_factory_.GetWeakPtr()));
}

ExtensionCacheImpl::~ExtensionCacheImpl() {
}

void ExtensionCacheImpl::Start(const base::Closure& callback) {
  if (!cache_ || cache_->is_ready()) {
    DCHECK(init_callbacks_.empty());
    callback.Run();
  } else {
    init_callbacks_.push_back(callback);
  }
}

void ExtensionCacheImpl::Shutdown(const base::Closure& callback) {
  if (cache_)
    cache_->Shutdown(callback);
  else
    callback.Run();
}

void ExtensionCacheImpl::AllowCaching(const std::string& id) {
  allowed_extensions_.insert(id);
}

bool ExtensionCacheImpl::GetExtension(const std::string& id,
                                      base::FilePath* file_path,
                                      std::string* version) {
  if (cache_)
    return cache_->GetExtension(id, file_path, version);
  else
    return false;
}

void ExtensionCacheImpl::PutExtension(const std::string& id,
                                      const base::FilePath& file_path,
                                      const std::string& version,
                                      const PutExtensionCallback& callback) {
  if (cache_ && ContainsKey(allowed_extensions_, id))
    cache_->PutExtension(id, file_path, version, callback);
  else
    callback.Run(file_path, true);
}

void ExtensionCacheImpl::OnCacheInitialized() {
  for (std::vector<base::Closure>::iterator it = init_callbacks_.begin();
       it != init_callbacks_.end(); ++it) {
    it->Run();
  }
  init_callbacks_.clear();

  uint64 cache_size = 0;
  size_t extensions_count = 0;
  if (cache_->GetStatistics(&cache_size, &extensions_count)) {
    UMA_HISTOGRAM_COUNTS_100("Extensions.ExtensionCacheCount",
                             extensions_count);
    UMA_HISTOGRAM_MEMORY_MB("Extensions.ExtensionCacheSize",
                            cache_size / (1024 * 1024));
  }
}

void ExtensionCacheImpl::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  if (!cache_)
    return;

  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR: {
      extensions::CrxInstaller* installer =
          content::Source<extensions::CrxInstaller>(source).ptr();
      // TODO(dpolukhin): remove extension from cache only if installation
      // failed due to file corruption.
      cache_->RemoveExtension(installer->expected_id());
      break;
    }

    default:
      NOTREACHED();
  }
}

}  // namespace extensions
