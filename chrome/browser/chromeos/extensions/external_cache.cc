// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_cache.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/updater/extension_downloader.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/extension.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

ExternalCache::ExternalCache(const base::FilePath& cache_dir,
                             net::URLRequestContextGetter* request_context,
                             const scoped_refptr<base::SequencedTaskRunner>&
                                 backend_task_runner,
                             Delegate* delegate,
                             bool always_check_updates,
                             bool wait_for_cache_initialization)
    : local_cache_(cache_dir, 0, base::TimeDelta(), backend_task_runner),
      request_context_(request_context),
      backend_task_runner_(backend_task_runner),
      delegate_(delegate),
      always_check_updates_(always_check_updates),
      wait_for_cache_initialization_(wait_for_cache_initialization),
      cached_extensions_(new base::DictionaryValue()),
      weak_ptr_factory_(this) {
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllBrowserContextsAndSources());
}

ExternalCache::~ExternalCache() {
}

void ExternalCache::Shutdown(const base::Closure& callback) {
  local_cache_.Shutdown(callback);
}

void ExternalCache::UpdateExtensionsList(
    scoped_ptr<base::DictionaryValue> prefs) {
  extensions_ = prefs.Pass();

  if (extensions_->empty()) {
    // If list of know extensions is empty, don't init cache on disk. It is
    // important shortcut for test to don't wait forever for cache dir
    // initialization that should happen outside of Chrome on real device.
    cached_extensions_->Clear();
    UpdateExtensionLoader();
    return;
  }

  if (local_cache_.is_uninitialized()) {
    local_cache_.Init(wait_for_cache_initialization_,
                      base::Bind(&ExternalCache::CheckCache,
                                 weak_ptr_factory_.GetWeakPtr()));
  } else {
    CheckCache();
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
      std::string id = it.key();
      LOG(ERROR) << "ExternalCache extension at " << path.value()
                 << " failed to install, deleting it.";
      cached_extensions_->Remove(id, NULL);
      extensions_->Remove(id, NULL);

      local_cache_.RemoveExtension(id);
      UpdateExtensionLoader();

      // Don't try to DownloadMissingExtensions() from here,
      // since it can cause a fail/retry loop.
      return;
    }
  }
  LOG(ERROR) << "ExternalCache cannot find external_crx " << path.value();
}

void ExternalCache::RemoveExtensions(const std::vector<std::string>& ids) {
  if (ids.empty())
    return;

  for (size_t i = 0; i < ids.size(); ++i) {
    cached_extensions_->Remove(ids[i], NULL);
    extensions_->Remove(ids[i], NULL);
    local_cache_.RemoveExtension(ids[i]);
  }
  UpdateExtensionLoader();
}

bool ExternalCache::GetExtension(const std::string& id,
                                 base::FilePath* file_path,
                                 std::string* version) {
  return local_cache_.GetExtension(id, file_path, version);
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
      delegate_->OnExtensionDownloadFailed(id, error);
    } else {
      // No version update for an already cached extension.
      delegate_->OnExtensionLoadedInCache(id);
    }
  } else {
    LOG(ERROR) << "ExternalCache failed to download extension " << id
               << ", error " << error;
    delegate_->OnExtensionDownloadFailed(id, error);
  }
}

void ExternalCache::OnExtensionDownloadFinished(
    const std::string& id,
    const base::FilePath& path,
    bool file_ownership_passed,
    const GURL& download_url,
    const std::string& version,
    const extensions::ExtensionDownloaderDelegate::PingResult& ping_result,
    const std::set<int>& request_ids) {
  DCHECK(file_ownership_passed);
  local_cache_.PutExtension(id, path, version,
                            base::Bind(&ExternalCache::OnPutExtension,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       id));
}

bool ExternalCache::IsExtensionPending(const std::string& id) {
  // Pending means that there is no installed version yet.
  return extensions_->HasKey(id) && !cached_extensions_->HasKey(id);
}

bool ExternalCache::GetExtensionExistingVersion(const std::string& id,
                                                std::string* version) {
  base::DictionaryValue* extension_dictionary = NULL;
  if (cached_extensions_->GetDictionary(id, &extension_dictionary)) {
    if (extension_dictionary->GetString(
            extensions::ExternalProviderImpl::kExternalVersion, version)) {
      return true;
    }
    *version = delegate_->GetInstalledExtensionVersion(id);
    return !version->empty();
  }
  return false;
}

void ExternalCache::UpdateExtensionLoader() {
  VLOG(1) << "Notify ExternalCache delegate about cache update";
  if (delegate_)
    delegate_->OnExtensionListsUpdated(cached_extensions_.get());
}

void ExternalCache::CheckCache() {
  if (local_cache_.is_shutdown())
    return;

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

    bool keep_if_present =
        entry->HasKey(extensions::ExternalProviderImpl::kKeepIfPresent);
    std::string external_update_url;
    entry->GetString(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                     &external_update_url);
    if (downloader_ && !keep_if_present) {
      GURL update_url;
      if (!external_update_url.empty())
        update_url = GURL(external_update_url);
      else if (always_check_updates_)
        update_url = extension_urls::GetWebstoreUpdateUrl();

      if (update_url.is_valid())
        downloader_->AddPendingExtension(it.key(), update_url, 0);
    }

    base::FilePath file_path;
    std::string version;
    if (local_cache_.GetExtension(it.key(), &file_path, &version)) {
      // Copy entry to don't modify it inside extensions_.
      base::DictionaryValue* entry_copy = entry->DeepCopy();

      if (extension_urls::IsWebstoreUpdateUrl(GURL(external_update_url))) {
        entry_copy->SetBoolean(
            extensions::ExternalProviderImpl::kIsFromWebstore, true);
      }
      entry_copy->Remove(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                         NULL);
      entry_copy->SetString(extensions::ExternalProviderImpl::kExternalVersion,
                            version);
      entry_copy->SetString(extensions::ExternalProviderImpl::kExternalCrx,
                            file_path.value());
      cached_extensions_->Set(it.key(), entry_copy);
    } else {
      bool has_external_crx = entry->HasKey(
          extensions::ExternalProviderImpl::kExternalCrx);
      bool is_already_installed =
          !delegate_->GetInstalledExtensionVersion(it.key()).empty();
      if (keep_if_present || has_external_crx || is_already_installed) {
        // Copy entry to don't modify it inside extensions_.
        cached_extensions_->Set(it.key(), entry->DeepCopy());
      }
    }
  }

  if (downloader_)
    downloader_->StartAllPending(NULL);

  VLOG(1) << "Updated ExternalCache, there are "
          << cached_extensions_->size() << " extensions cached";

  UpdateExtensionLoader();
}

void ExternalCache::OnPutExtension(const std::string& id,
                                   const base::FilePath& file_path,
                                   bool file_ownership_passed) {
  if (local_cache_.is_shutdown() || file_ownership_passed) {
    backend_task_runner_->PostTask(FROM_HERE,
        base::Bind(base::IgnoreResult(&base::DeleteFile), file_path, true));
    return;
  }

  VLOG(1) << "ExternalCache installed a new extension in the cache " << id;

  base::DictionaryValue* entry = NULL;
  if (!extensions_->GetDictionary(id, &entry)) {
    LOG(ERROR) << "ExternalCache cannot find entry for extension " << id;
    return;
  }

  // Copy entry to don't modify it inside extensions_.
  entry = entry->DeepCopy();

  std::string version;
  if (!local_cache_.GetExtension(id, NULL, &version)) {
    // Copy entry to don't modify it inside extensions_.
    LOG(ERROR) << "Can't find installed extension in cache " << id;
    return;
  }

  std::string update_url;
  if (entry->GetString(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                       &update_url) &&
      extension_urls::IsWebstoreUpdateUrl(GURL(update_url))) {
    entry->SetBoolean(extensions::ExternalProviderImpl::kIsFromWebstore, true);
  }
  entry->Remove(extensions::ExternalProviderImpl::kExternalUpdateUrl, NULL);
  entry->SetString(extensions::ExternalProviderImpl::kExternalVersion, version);
  entry->SetString(extensions::ExternalProviderImpl::kExternalCrx,
                   file_path.value());

  cached_extensions_->Set(id, entry);
  if (delegate_)
    delegate_->OnExtensionLoadedInCache(id);
  UpdateExtensionLoader();
}

std::string ExternalCache::Delegate::GetInstalledExtensionVersion(
    const std::string& id) {
  return std::string();
}

}  // namespace chromeos
