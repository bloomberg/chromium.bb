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
#include "chrome/browser/extensions/updater/local_extension_cache.h"
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
    // Called after extension with |id| is loaded in cache.
    virtual void OnExtensionLoadedInCache(const std::string& id) {}
    // Called when extension with |id| is failed with downloading for |error|.
    virtual void OnExtensionDownloadFailed(
        const std::string& id,
        extensions::ExtensionDownloaderDelegate::Error error) {}

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
      bool file_ownership_passed,
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

  // Removes extensions listed in |ids| from external cache, corresponding crx
  // files will be removed from disk too.
  void RemoveExtensions(const std::vector<std::string>& ids);

  // If extension with |id| exists in the cache, returns |true|, |file_path| and
  // |version| for the extension. Extension will be marked as used with current
  // timestamp.
  bool GetExtension(const std::string& id,
                    base::FilePath* file_path,
                    std::string* version);

 private:
  // Notifies the that the cache has been updated, providing
  // extensions loader with an updated list of extensions.
  void UpdateExtensionLoader();

  // Checks the cache contents and initiate download if needed.
  void CheckCache();

  // Invoked on the UI thread when a new entry has been installed in the cache.
  void OnPutExtension(const std::string& id,
                      const base::FilePath& file_path,
                      bool file_ownership_passed);

  extensions::LocalExtensionCache local_cache_;

  // Request context used by the |downloader_|.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // Task runner for executing file I/O tasks.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Delegate that would like to get notifications about cache updates.
  Delegate* delegate_;

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

  // Observes failures to install CRX files.
  content::NotificationRegistrar notification_registrar_;

  // Weak factory for callbacks.
  base::WeakPtrFactory<ExternalCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalCache);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_H_
