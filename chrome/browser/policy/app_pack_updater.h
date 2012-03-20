// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_APP_PACK_UPDATER_H_
#define CHROME_BROWSER_POLICY_APP_PACK_UPDATER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/updater/extension_downloader_delegate.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "content/public/browser/notification_observer.h"

class ExternalExtensionLoader;
class FilePath;
class GURL;

namespace extensions {
class ExtensionDownloader;
}

namespace net {
class URLRequestContextGetter;
}

namespace tracked_objects {
class Location;
}

namespace policy {

class AppPackExternalExtensionLoader;
class BrowserPolicyConnector;

// The AppPackUpdater manages a set of extensions that are configured via a
// device policy to be locally cached and installed into the Demo user account
// at login time.
class AppPackUpdater : public CloudPolicySubsystem::Observer,
                       public content::NotificationObserver,
                       public extensions::ExtensionDownloaderDelegate {
 public:
  // Keys for the entries in the AppPack dictionary policy.
  static const char kExtensionId[];
  static const char kUpdateUrl[];

  // The |request_context| is used for the update checks.
  AppPackUpdater(net::URLRequestContextGetter* request_context,
                 BrowserPolicyConnector* connector);
  virtual ~AppPackUpdater();

  // Creates an ExternalExtensionLoader that will load the crx files downloaded
  // by the AppPackUpdater. This can be called at most once, and the caller
  // owns the returned value.
  ExternalExtensionLoader* CreateExternalExtensionLoader();

 private:
  struct CacheEntry {
    std::string path;
    std::string cached_version;
  };

  // Maps an extension ID to its update URL.
  typedef std::map<std::string, std::string> PolicyEntryMap;

  // Maps an extension ID to a CacheEntry.
  typedef std::map<std::string, CacheEntry> CacheEntryMap;

  void Init();

  // CloudPolicySubsystem::Observer:
  virtual void OnPolicyStateChanged(
      CloudPolicySubsystem::PolicySubsystemState state,
      CloudPolicySubsystem::ErrorDetails error_details) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Loads the current policy and schedules a cache update.
  void LoadPolicy();

  // Starts a cache update check immediately.
  void CheckCacheNow();

  // Performs a cache update check on the blocking pool. |app_pack_updater| is
  // used to reply in the UI thread. |valid_ids| contains the list of IDs that
  // are currently configured by policy; anything else is invalid, and should
  // be removed from the cache. |valid_ids| is owned by the posted task.
  static void BlockingCheckCache(base::WeakPtr<AppPackUpdater> app_pack_updater,
                                 const std::set<std::string>* valid_ids);

  // Helper for BlockingCheckCache().
  static void BlockingCheckCacheInternal(
      const std::set<std::string>* valid_ids,
      CacheEntryMap* entries);

  // Invoked when the cache has been updated. |cache_entries| contains all the
  // currently valid crx files in the cache, and is owned by the posted task.
  void OnCacheUpdated(CacheEntryMap* cache_entries);

  // Notifies the |extension_loader_| that the cache has been updated, providing
  // it with an updated list of app-pack extensions.
  void UpdateExtensionLoader();

  // Schedules downloads of all the extensions that are currently configured
  // by the policy but missing in the cache.
  void DownloadMissingExtensions();

  // Implementation of ExtensionDownloaderDelegate:

  virtual void OnExtensionDownloadFailed(
      const std::string& id,
      Error error,
      const PingResult& ping_result) OVERRIDE;

  virtual void OnExtensionDownloadFinished(
      const std::string& id,
      const FilePath& path,
      const GURL& download_url,
      const std::string& version,
      const PingResult& ping_result) OVERRIDE;

  virtual void OnBlacklistDownloadFinished(
      const std::string& data,
      const std::string& package_hash,
      const std::string& version,
      const PingResult& ping_result) OVERRIDE;

  virtual bool IsExtensionPending(const std::string& id) OVERRIDE;

  virtual bool GetExtensionExistingVersion(const std::string& id,
                                           std::string* version) OVERRIDE;

  // Invoked to install the downloaded crx file at |path| in the AppPack cache.
  static void BlockingInstallCacheEntry(
      base::WeakPtr<AppPackUpdater> app_pack_updater,
      const std::string& id,
      const FilePath& path,
      const std::string& version);

  // Invoked on the UI thread when a new AppPack entry has been installed in
  // the AppPack cache.
  void OnCacheEntryInstalled(const std::string& id,
                             const std::string& path,
                             const std::string& version);

  // Helper to post blocking IO tasks to the blocking pool.
  void PostBlockingTask(const tracked_objects::Location& from_here,
                        const base::Closure& task);

  base::WeakPtrFactory<AppPackUpdater> weak_ptr_factory_;

  // Observes updates to the |device_cloud_policy_subsystem_|, to detect
  // device enrollment.
  scoped_ptr<CloudPolicySubsystem::ObserverRegistrar> registrar_;

  // Unique sequence token so that tasks posted by the AppPackUpdater are
  // executed sequentially in the blocking pool.
  base::SequencedWorkerPool::SequenceToken worker_pool_token_;

  // This is the list of extensions currently configured by the policy.
  PolicyEntryMap app_pack_extensions_;

  // This contains extensions that are both currently configured by the policy
  // and that have a valid crx in the cache.
  CacheEntryMap cached_extensions_;

  // The extension loader wires the AppPackUpdater to the extensions system, and
  // makes it install the currently cached extensions.
  bool created_extension_loader_;
  base::WeakPtr<AppPackExternalExtensionLoader> extension_loader_;

  // Used to download the extensions configured via policy, and to check for
  // updates.
  scoped_ptr<extensions::ExtensionDownloader> downloader_;

  // Request context used by the |downloader_|.
  net::URLRequestContextGetter* request_context_;

  DISALLOW_COPY_AND_ASSIGN(AppPackUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_APP_PACK_UPDATER_H_
