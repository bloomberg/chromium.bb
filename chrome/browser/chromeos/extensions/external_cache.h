// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/updater/extension_downloader_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class DictionaryValue;
}

namespace extensions {
class ExtensionDownloader;
}

namespace net {
class URLRequestContextGetter;
}

namespace tracked_objects {
class Location;
}

namespace chromeos {

// The ExternalCache manages cache for external extensions.
class ExternalCache : public content::NotificationObserver,
                      public extensions::ExtensionDownloaderDelegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Caller owns |prefs|.
    virtual void OnExtensionListsUpdated(
        const base::DictionaryValue* prefs) = 0;
  };

  // The |request_context| is used for the update checks.
  // By default updates are checked for the extensions with external_update_url.
  // If |always_check_webstore| set, updates will be check for external_crx too.
  // If |wait_cache_initialization|, cache will wait for flag file in cache dir.
  ExternalCache(const std::string& cache_dir,
                net::URLRequestContextGetter* request_context,
                Delegate* delegate,
                bool always_check_updates,
                bool wait_cache_initialization);
  virtual ~ExternalCache();

  // Returns already cached extensions.
  const base::DictionaryValue* cached_extensions() {
    return cached_extensions_.get();
  }

  // Update list of extensions in cache and force update check for them.
  // ExternalCache gets ownership of |prefs|.
  void UpdateExtensionsList(scoped_ptr<base::DictionaryValue> prefs);

  // If a user of one of the ExternalCache's extensions detects that
  // the extension is damaged then this method can be used to remove it from
  // the cache and retry to download it after a restart.
  void OnDamagedFileDetected(const base::FilePath& path);

 protected:
  // Implementation of content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Implementation of ExtensionDownloaderDelegate:
  virtual void OnExtensionDownloadFailed(
      const std::string& id,
      Error error,
      const PingResult& ping_result,
      const std::set<int>& request_ids) OVERRIDE;

  virtual void OnExtensionDownloadFinished(
      const std::string& id,
      const base::FilePath& path,
      const GURL& download_url,
      const std::string& version,
      const PingResult& ping_result,
      const std::set<int>& request_ids) OVERRIDE;

  virtual bool IsExtensionPending(const std::string& id) OVERRIDE;

  virtual bool GetExtensionExistingVersion(const std::string& id,
                                           std::string* version) OVERRIDE;

  // Starts a cache update check immediately.
  void CheckCacheNow();

  // Notifies the that the cache has been updated, providing
  // extensions loader with an updated list of extensions.
  void UpdateExtensionLoader();

  // Performs a cache update check on the blocking pool. |external_cache| is
  // used to reply in the UI thread. |prefs| contains the list extensions
  // anything else is invalid, and should be removed from the cache.
  // Ownership of |prefs| is transferred to this function.
  static void BlockingCheckCache(
      base::WeakPtr<ExternalCache> external_cache,
      base::SequencedWorkerPool::SequenceToken token,
      const std::string& app_cache_dir,
      scoped_ptr<base::DictionaryValue> prefs,
      bool wait_cache_initialization);

  // Helper for BlockingCheckCache(), updates |prefs|.
  static void BlockingCheckCacheInternal(
      const std::string& app_cache_dir,
      base::DictionaryValue* prefs);

  // Invoked when the cache has been updated. |prefs| contains all the currently
  // valid crx files in the cache, ownerships is transfered to this function.
  void OnCacheUpdated(scoped_ptr<base::DictionaryValue> prefs);

  // Invoked to install the downloaded crx file at |path| in the cache.
  static void BlockingInstallCacheEntry(
      base::WeakPtr<ExternalCache> external_cache,
      const std::string& app_cache_dir,
      const std::string& id,
      const base::FilePath& path,
      const std::string& version);

  // Invoked on the UI thread when a new entry has been installed in the cache.
  void OnCacheEntryInstalled(const std::string& id,
                             const std::string& path,
                             const std::string& version);

  // Helper to post blocking IO tasks to the blocking pool.
  void PostBlockingTask(const tracked_objects::Location& from_here,
                        const base::Closure& task);

  // Path to the dir where apps cache is stored.
  std::string cache_dir_;

  // Request context used by the |downloader_|.
  net::URLRequestContextGetter* request_context_;

  // Delegate that would like to get notifications about cache updates.
  Delegate* delegate_;

  // Updates needs to be check for the extensions with external_crx too.
  bool always_check_updates_;

  // Set to true if cache should wait for initialization flag file.
  bool wait_cache_initialization_;

  // This is the list of extensions currently configured.
  scoped_ptr<base::DictionaryValue> extensions_;

  // This contains extensions that are both currently configured
  // and that have a valid crx in the cache.
  scoped_ptr<base::DictionaryValue> cached_extensions_;

  // Used to download the extensions and to check for updates.
  scoped_ptr<extensions::ExtensionDownloader> downloader_;

  base::WeakPtrFactory<ExternalCache> weak_ptr_factory_;

  // Observes failures to install CRX files.
  content::NotificationRegistrar notification_registrar_;

  // Unique sequence token so that tasks posted by the ExternalCache are
  // executed sequentially in the blocking pool.
  base::SequencedWorkerPool::SequenceToken worker_pool_token_;

  DISALLOW_COPY_AND_ASSIGN(ExternalCache);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_H_
