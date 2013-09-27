// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_cache.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/updater/extension_downloader.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace chromeos {

namespace {

// File name extension for CRX files (not case sensitive).
const char kCRXFileExtension[] = ".crx";

// Name of flag file that indicates that cache is ready (import finished).
const char kCacheReadyFlagFileName[] = ".initialized";

// Delay between checking cache ready flag file.
const int64_t kCacheReadyDelayMs = 1000;

}  // namespace

ExternalCache::ExternalCache(const std::string& cache_dir,
                             net::URLRequestContextGetter* request_context,
                             Delegate* delegate,
                             bool always_check_updates,
                             bool wait_cache_initialization)
    : cache_dir_(cache_dir),
      request_context_(request_context),
      delegate_(delegate),
      always_check_updates_(always_check_updates),
      wait_cache_initialization_(wait_cache_initialization),
      cached_extensions_(new base::DictionaryValue()),
      weak_ptr_factory_(this),
      worker_pool_token_(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken()) {
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllBrowserContextsAndSources());
}

ExternalCache::~ExternalCache() {
}

void ExternalCache::UpdateExtensionsList(
    scoped_ptr<base::DictionaryValue> prefs) {
  extensions_ = prefs.Pass();
  if (extensions_->empty()) {
    // Don't check cache and clear it if there are no extensions in the list.
    // It is important case because login to supervised user shouldn't clear
    // cache for normal users.
    // TODO(dpolukhin): introduce reference counting to preserve cache elements
    // when they are not needed for current user but needed for other users.
    cached_extensions_->Clear();
    UpdateExtensionLoader();
  } else {
    CheckCacheNow();
  }
}

void ExternalCache::OnDamagedFileDetected(const base::FilePath& path) {
  for (base::DictionaryValue::Iterator it(*cached_extensions_.get());
       !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* entry = NULL;
    if (!it.value().GetAsDictionary(&entry)) {
      NOTREACHED() << "ExternalCache found bad entry with type "
                   << it.value().GetType();
      continue;
    }

    std::string external_crx;
    if (entry->GetString(extensions::ExternalProviderImpl::kExternalCrx,
                         &external_crx) &&
        external_crx == path.value()) {

      LOG(ERROR) << "ExternalCache extension at " << path.value()
                 << " failed to install, deleting it.";
      cached_extensions_->Remove(it.key(), NULL);
      UpdateExtensionLoader();

      // The file will be downloaded again on the next restart.
      if (base::FilePath(cache_dir_).IsParent(path)) {
        // Don't delete files out of cache_dir_.
        content::BrowserThread::PostTask(
            content::BrowserThread::FILE, FROM_HERE,
            base::Bind(base::IgnoreResult(base::DeleteFile), path, true));
      }

      // Don't try to DownloadMissingExtensions() from here,
      // since it can cause a fail/retry loop.
      return;
    }
  }
  LOG(ERROR) << "ExternalCache cannot find external_crx " << path.value();
}

void ExternalCache::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR: {
      extensions::CrxInstaller* installer =
          content::Source<extensions::CrxInstaller>(source).ptr();
      OnDamagedFileDetected(installer->source_file());
      break;
    }

    default:
      NOTREACHED();
  }
}

void ExternalCache::OnExtensionDownloadFailed(
    const std::string& id,
    extensions::ExtensionDownloaderDelegate::Error error,
    const extensions::ExtensionDownloaderDelegate::PingResult& ping_result,
    const std::set<int>& request_ids) {
  if (error == NO_UPDATE_AVAILABLE) {
    if (!cached_extensions_->HasKey(id)) {
      LOG(ERROR) << "ExternalCache extension " << id
                 << " not found on update server";
    }
  } else {
    LOG(ERROR) << "ExternalCache failed to download extension " << id
               << ", error " << error;
  }
}

void ExternalCache::OnExtensionDownloadFinished(
    const std::string& id,
    const base::FilePath& path,
    const GURL& download_url,
    const std::string& version,
    const extensions::ExtensionDownloaderDelegate::PingResult& ping_result,
    const std::set<int>& request_ids) {
  // The explicit copy ctors are to make sure that Bind() binds a copy and not
  // a reference to the arguments.
  PostBlockingTask(FROM_HERE,
                   base::Bind(&ExternalCache::BlockingInstallCacheEntry,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::string(cache_dir_),
                              std::string(id),
                              base::FilePath(path),
                              std::string(version)));
}

bool ExternalCache::IsExtensionPending(const std::string& id) {
  // Pending means that there is no installed version yet.
  return extensions_->HasKey(id) && !cached_extensions_->HasKey(id);
}

bool ExternalCache::GetExtensionExistingVersion(const std::string& id,
                                                std::string* version) {
  DictionaryValue* extension_dictionary = NULL;
  if (cached_extensions_->GetDictionary(id, &extension_dictionary)) {
    return extension_dictionary->GetString(
        extensions::ExternalProviderImpl::kExternalVersion,
        version);
  }
  return false;
}

void ExternalCache::CheckCacheNow() {
  scoped_ptr<DictionaryValue> prefs(extensions_->DeepCopy());
  PostBlockingTask(FROM_HERE,
                   base::Bind(&ExternalCache::BlockingCheckCache,
                              weak_ptr_factory_.GetWeakPtr(),
                              worker_pool_token_,
                              std::string(cache_dir_),
                              base::Passed(&prefs),
                              wait_cache_initialization_));
}

void ExternalCache::UpdateExtensionLoader() {
  VLOG(1) << "Notify ExternalCache delegate about cache update";
  if (delegate_)
    delegate_->OnExtensionListsUpdated(cached_extensions_.get());
}

// static
void ExternalCache::BlockingCheckCache(
    base::WeakPtr<ExternalCache> external_cache,
    base::SequencedWorkerPool::SequenceToken sequence_token,
    const std::string& cache_dir,
    scoped_ptr<base::DictionaryValue> prefs,
    bool wait_cache_initialization) {

  base::FilePath dir(cache_dir);
  if (wait_cache_initialization &&
      !base::PathExists(dir.AppendASCII(kCacheReadyFlagFileName))) {
    content::BrowserThread::GetBlockingPool()->PostDelayedSequencedWorkerTask(
        sequence_token,
        FROM_HERE,
        base::Bind(&ExternalCache::BlockingCheckCache,
                   external_cache,
                   sequence_token,
                   std::string(cache_dir),
                   base::Passed(&prefs),
                   wait_cache_initialization),
        base::TimeDelta::FromMilliseconds(kCacheReadyDelayMs));
    return;
  }

  BlockingCheckCacheInternal(cache_dir, prefs.get());
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                          base::Bind(&ExternalCache::OnCacheUpdated,
                                     external_cache,
                                     base::Passed(&prefs)));
}

// static
void ExternalCache::BlockingCheckCacheInternal(const std::string& cache_dir,
                                               base::DictionaryValue* prefs) {
  // Start by verifying that the cache dir exists.
  base::FilePath dir(cache_dir);
  if (!base::DirectoryExists(dir)) {
    // Create it now.
    if (!file_util::CreateDirectory(dir)) {
      LOG(ERROR) << "Failed to create ExternalCache directory at "
                 << dir.value();
    }

    // Nothing else to do. Cache won't be used.
    return;
  }

  // Enumerate all the files in the cache |dir|, including directories
  // and symlinks. Each unrecognized file will be erased.
  int types = base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
      base::FileEnumerator::SHOW_SYM_LINKS;
  base::FileEnumerator enumerator(dir, false /* recursive */, types);
  for (base::FilePath path = enumerator.Next();
       !path.empty(); path = enumerator.Next()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    std::string basename = path.BaseName().value();

    if (info.IsDirectory() || file_util::IsLink(info.GetName())) {
      LOG(ERROR) << "Erasing bad file in ExternalCache directory: " << basename;
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

    base::DictionaryValue* entry = NULL;
    if (!extensions::Extension::IdIsValid(id)) {
      LOG(ERROR) << "Bad extension id in ExternalCache: " << id;
      id.clear();
    } else if (!prefs->GetDictionary(id, &entry)) {
      LOG(WARNING) << basename << " is in the cache but is not configured by "
                   << "the ExternalCache source, and will be erased.";
      id.clear();
    }

    if (!Version(version).IsValid()) {
      LOG(ERROR) << "Bad extension version in ExternalCache: " << version;
      version.clear();
    }

    if (id.empty() || version.empty()) {
      LOG(ERROR) << "Invalid file in ExternalCache, erasing: " << basename;
      base::DeleteFile(path, true /* recursive */);
      continue;
    }

    // Enforce a lower-case id.
    id = StringToLowerASCII(id);

    std::string update_url;
    std::string prev_version_string;
    std::string prev_crx;
    if (entry->GetString(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                         &update_url)) {
      VLOG(1) << "ExternalCache found cached version " << version
              << " for extension id: " << id;
      entry->Remove(extensions::ExternalProviderImpl::kExternalUpdateUrl, NULL);
      entry->SetString(extensions::ExternalProviderImpl::kExternalVersion,
                       version);
      entry->SetString(extensions::ExternalProviderImpl::kExternalCrx,
                       path.value());
      if (extension_urls::IsWebstoreUpdateUrl(GURL(update_url))) {
        entry->SetBoolean(extensions::ExternalProviderImpl::kIsFromWebstore,
                          true);
      }
    } else if (
        entry->GetString(extensions::ExternalProviderImpl::kExternalVersion,
                         &prev_version_string) &&
        entry->GetString(extensions::ExternalProviderImpl::kExternalCrx,
                         &prev_crx)) {
      Version prev_version(prev_version_string);
      Version curr_version(version);
      DCHECK(prev_version.IsValid());
      DCHECK(curr_version.IsValid());
      if (prev_version.CompareTo(curr_version) <= 0) {
        VLOG(1) << "ExternalCache found old cached version "
                << prev_version_string << " path: " << prev_crx;
        base::FilePath prev_crx_file(prev_crx);
        if (dir.IsParent(prev_crx_file)) {
          // Only delete old cached files under cache_dir_ folder.
          base::DeleteFile(base::FilePath(prev_crx), true /* recursive */);
        }
        entry->SetString(extensions::ExternalProviderImpl::kExternalVersion,
                         version);
        entry->SetString(extensions::ExternalProviderImpl::kExternalCrx,
                         path.value());
      } else {
        VLOG(1) << "ExternalCache found old cached version "
                << version << " path: " << path.value();
        base::DeleteFile(path, true /* recursive */);
      }
    } else {
      NOTREACHED() << "ExternalCache found bad entry for extension id: " << id
                   << " file path: " << path.value();
    }
  }
}

void ExternalCache::OnCacheUpdated(scoped_ptr<base::DictionaryValue> prefs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // If request_context_ is missing we can't download anything.
  if (!downloader_ && request_context_) {
    downloader_.reset(
        new extensions::ExtensionDownloader(this, request_context_));
  }

  cached_extensions_->Clear();
  for (base::DictionaryValue::Iterator it(*extensions_.get());
       !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* entry = NULL;
    if (!it.value().GetAsDictionary(&entry)) {
      LOG(ERROR) << "ExternalCache found bad entry with type "
                 << it.value().GetType();
      continue;
    }

    // Check for updates for all extensions configured except for extensions
    // marked as keep_if_present.
    if (downloader_ &&
        !entry->HasKey(extensions::ExternalProviderImpl::kKeepIfPresent)) {
      GURL update_url;
      std::string external_update_url;
      if (entry->GetString(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                           &external_update_url)) {
        update_url = GURL(external_update_url);
      } else if (always_check_updates_) {
        update_url = extension_urls::GetWebstoreUpdateUrl();
      }
      if (update_url.is_valid())
        downloader_->AddPendingExtension(it.key(), update_url, 0);
    }

    base::DictionaryValue* cached_entry = NULL;
    if (prefs->GetDictionary(it.key(), &cached_entry)) {
      std::string crx_path;
      if (!downloader_ ||
          cached_entry->GetString(
              extensions::ExternalProviderImpl::kExternalCrx, &crx_path) ||
          cached_entry->HasKey(
              extensions::ExternalProviderImpl::kKeepIfPresent)) {
        scoped_ptr<base::Value> value;
        prefs->Remove(it.key(), &value);
        cached_extensions_->Set(it.key(), value.release());
      }
    }
  }
  if (downloader_)
    downloader_->StartAllPending();

  VLOG(1) << "Updated ExternalCache, there are "
          << cached_extensions_->size() << " extensions cached";

  UpdateExtensionLoader();
}

// static
void ExternalCache::BlockingInstallCacheEntry(
    base::WeakPtr<ExternalCache> external_cache,
    const std::string& app_cache_dir,
    const std::string& id,
    const base::FilePath& path,
    const std::string& version) {
  Version version_validator(version);
  if (!version_validator.IsValid()) {
    LOG(ERROR) << "ExternalCache downloaded extension " << id << " but got bad "
               << "version: " << version;
    base::DeleteFile(path, true /* recursive */);
    return;
  }

  std::string basename = id + "-" + version + kCRXFileExtension;
  base::FilePath cache_dir(app_cache_dir);
  base::FilePath cached_crx_path = cache_dir.Append(basename);

  if (base::PathExists(cached_crx_path)) {
    LOG(WARNING) << "AppPack downloaded a crx whose filename will overwrite "
                 << "an existing cached crx.";
    base::DeleteFile(cached_crx_path, true /* recursive */);
  }

  if (!base::DirectoryExists(cache_dir)) {
    LOG(ERROR) << "AppPack cache directory does not exist, creating now: "
               << cache_dir.value();
    if (!file_util::CreateDirectory(cache_dir)) {
      LOG(ERROR) << "Failed to create the AppPack cache dir!";
      base::DeleteFile(path, true /* recursive */);
      return;
    }
  }

  if (!base::Move(path, cached_crx_path)) {
    LOG(ERROR) << "Failed to move AppPack crx from " << path.value()
               << " to " << cached_crx_path.value();
    base::DeleteFile(path, true /* recursive */);
    return;
  }

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                          base::Bind(&ExternalCache::OnCacheEntryInstalled,
                                     external_cache,
                                     std::string(id),
                                     cached_crx_path.value(),
                                     std::string(version)));
}

void ExternalCache::OnCacheEntryInstalled(const std::string& id,
                                          const std::string& path,
                                          const std::string& version) {
  VLOG(1) << "AppPack installed a new extension in the cache: " << path;

  base::DictionaryValue* entry = NULL;
  if (!extensions_->GetDictionary(id, &entry)) {
    LOG(ERROR) << "ExternalCache cannot find entry for extension " << id;
    return;
  }

  // Copy entry to don't modify it inside extensions_.
  entry = entry->DeepCopy();

  std::string update_url;
  if (entry->GetString(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                       &update_url) &&
      extension_urls::IsWebstoreUpdateUrl(GURL(update_url))) {
    entry->SetBoolean(extensions::ExternalProviderImpl::kIsFromWebstore, true);
  }
  entry->Remove(extensions::ExternalProviderImpl::kExternalUpdateUrl, NULL);
  entry->SetString(extensions::ExternalProviderImpl::kExternalVersion, version);
  entry->SetString(extensions::ExternalProviderImpl::kExternalCrx, path);

  cached_extensions_->Set(id, entry);
  UpdateExtensionLoader();
}

void ExternalCache::PostBlockingTask(const tracked_objects::Location& location,
                                     const base::Closure& task) {
  content::BrowserThread::GetBlockingPool()->
      PostSequencedWorkerTaskWithShutdownBehavior(
          worker_pool_token_, location, task,
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

}  // namespace chromeos
