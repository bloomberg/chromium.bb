// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
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

namespace chromeos {

// The ExternalCache manages a cache for external extensions.
class ExternalCache : public content::NotificationObserver,
                      public extensions::ExtensionDownloaderDelegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Caller owns |prefs|.
    virtual void OnExtensionListsUpdated(
        const base::DictionaryValue* prefs) = 0;

    // Cache needs to provide already installed extensions otherwise they
    // will be removed. Cache calls this function to get version of installed
    // extension or empty string if not installed.
    virtual std::string GetInstalledExtensionVersion(const std::string& id);
  };

  // The |request_context| is used for update checks. All file I/O is done via
  // the |backend_task_runner|. If |always_check_updates| is |false|, update
  // checks are performed for extensions that have an |external_update_url|
  // only. If |wait_for_cache_initialization| is |true|, the cache contents will
  // not be read until a flag file appears in the cache directory, signaling
  // that the cache is ready.
  ExternalCache(const base::FilePath& cache_dir,
                net::URLRequestContextGetter* request_context,
                const scoped_refptr<base::SequencedTaskRunner>&
                    backend_task_runner,
                Delegate* delegate,
                bool always_check_updates,
                bool wait_for_cache_initialization);
  virtual ~ExternalCache();

  // Name of flag file that indicates that cache is ready (import finished).
  static const char kCacheReadyFlagFileName[];

  // Returns already cached extensions.
  const base::DictionaryValue* cached_extensions() {
    return cached_extensions_.get();
  }

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

  // Shut down the cache. The |callback| will be invoked when the cache has shut
  // down completely and there are no more pending file I/O operations.
  void Shutdown(const base::Closure& callback);

  // Replace the list of extensions to cache with |prefs| and perform update
  // checks for these.
  void UpdateExtensionsList(scoped_ptr<base::DictionaryValue> prefs);

  // If a user of one of the ExternalCache's extensions detects that
  // the extension is damaged then this method can be used to remove it from
  // the cache and retry to download it after a restart.
  void OnDamagedFileDetected(const base::FilePath& path);

 private:
  // Notifies the that the cache has been updated, providing
  // extensions loader with an updated list of extensions.
  void UpdateExtensionLoader();

  // Checks the cache contents and deletes any entries no longer referenced by
  // |extensions_|. If |wait_for_cache_initialization_| is |true|, waits for the
  // cache to become ready first, as indicated by the presence of a flag file.
  void CheckCache();

  // Checks whether a flag file exists in the |cache_dir|, indicating that the
  // cache is ready. This method is invoked via the |backend_task_runner_| and
  // posts its result back to the |external_cache| on the UI thread.
  static void BackendCheckCacheStatus(
      base::WeakPtr<ExternalCache> external_cache,
      const base::FilePath& cache_dir);

  // Invoked on the UI thread after checking whether the cache is ready. If the
  // cache is not ready yet, posts a delayed task that will repeat the check,
  // thus polling for cache readiness.
  void OnCacheStatusChecked(bool ready);

  // Checks the cache contents. This is a helper that invokes the actual check
  // by posting to the |backend_task_runner_|.
  void CheckCacheContents();

  // Checks the cache contents, deleting any entries no longer referenced by
  // |prefs|. This method is invoked via the |backend_task_runner_| and posts
  // back a list of cache entries to the |external_cache| on the UI thread.
  static void BackendCheckCacheContents(
      base::WeakPtr<ExternalCache> external_cache,
      const base::FilePath& cache_dir,
      scoped_ptr<base::DictionaryValue> prefs);

  // Helper for BackendCheckCacheContents() that updates |prefs|.
  static void BackendCheckCacheContentsInternal(
      const base::FilePath& cache_dir,
      base::DictionaryValue* prefs);

  // Invoked when the cache has been updated. |prefs| contains all the currently
  // valid crx files in the cache, ownerships is transfered to this function.
  void OnCacheUpdated(scoped_ptr<base::DictionaryValue> prefs);

  // Installs the downloaded crx file at |path| in the |cache_dir|. This method
  // is invoked via the |backend_task_runner_|.
  static void BackendInstallCacheEntry(
      base::WeakPtr<ExternalCache> external_cache,
      const base::FilePath& cache_dir,
      const std::string& id,
      const base::FilePath& path,
      const std::string& version);

  // Invoked on the UI thread when a new entry has been installed in the cache.
  void OnCacheEntryInstalled(const std::string& id,
                             const base::FilePath& path,
                             const std::string& version);

  // Posted to the |backend_task_runner_| during cache shutdown so that it runs
  // after all file I/O has been completed. Invokes |callback| on the UI thread
  // to indicate that the cache has been shut down completely.
  static void BackendShudown(const base::Closure& callback);

  // Path to the directory where the extension cache is stored.
  base::FilePath cache_dir_;

  // Request context used by the |downloader_|.
  net::URLRequestContextGetter* request_context_;

  // Delegate that would like to get notifications about cache updates.
  Delegate* delegate_;

  // Whether the cache shutdown has been initiated.
  bool shutdown_;

  // Updates needs to be check for the extensions with external_crx too.
  bool always_check_updates_;

  // Set to true if cache should wait for initialization flag file.
  bool wait_for_cache_initialization_;

  // This is the list of extensions currently configured.
  scoped_ptr<base::DictionaryValue> extensions_;

  // This contains extensions that are both currently configured
  // and that have a valid crx in the cache.
  scoped_ptr<base::DictionaryValue> cached_extensions_;

  // Used to download the extensions and to check for updates.
  scoped_ptr<extensions::ExtensionDownloader> downloader_;

  // Task runner for executing file I/O tasks.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Observes failures to install CRX files.
  content::NotificationRegistrar notification_registrar_;

  // Weak factory for callbacks from the backend and delayed tasks.
  base::WeakPtrFactory<ExternalCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalCache);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_H_
