// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/containers/stack.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/browser/updater/update_service.h"
#include "url/gurl.h"

class ExtensionServiceInterface;
class PrefService;
class Profile;

namespace extensions {

class ExtensionCache;
class ExtensionPrefs;
class ExtensionSet;
struct ExtensionUpdateCheckParams;
class ExtensionUpdaterTest;

// A class for doing auto-updates of installed Extensions. Used like this:
//
// std::unique_ptr<ExtensionUpdater> updater =
//    std::make_unique<ExtensionUpdater>(my_extensions_service,
//                                       extension_prefs,
//                                       pref_service,
//                                       profile,
//                                       update_frequency_secs,
//                                       downloader_factory);
// updater->Start();
// ....
// updater->Stop();
class ExtensionUpdater : public ExtensionDownloaderDelegate,
                         public content::NotificationObserver {
 public:
  // TODO(mxnguyen): Change FinishedCallback to base::OnceClosure.
  typedef base::Closure FinishedCallback;

  struct CheckParams {
    // Creates a default CheckParams instance that checks for all extensions.
    CheckParams();
    ~CheckParams();

    // The set of extensions that should be checked for updates. If empty
    // all extensions will be included in the update check.
    std::list<std::string> ids;

    // Normally extension updates get installed only when the extension is idle.
    // Setting this to true causes any updates that are found to be installed
    // right away.
    bool install_immediately;

    // An extension update check can be originated by a user or by a timer.
    // When the value of |fetch_priority| is FOREGROUND, the update request was
    // initiated by a user.
    ManifestFetchData::FetchPriority fetch_priority;

    // Callback to call when the update check is complete. Can be null, if
    // you're not interested in when this happens.
    FinishedCallback callback;
  };

  // Holds a pointer to the passed |service|, using it for querying installed
  // extensions and installing updated ones. The |frequency_seconds| parameter
  // controls how often update checks are scheduled.
  ExtensionUpdater(ExtensionServiceInterface* service,
                   ExtensionPrefs* extension_prefs,
                   PrefService* prefs,
                   Profile* profile,
                   int frequency_seconds,
                   ExtensionCache* cache,
                   const ExtensionDownloader::Factory& downloader_factory);

  ~ExtensionUpdater() override;

  // Starts the updater running.  Should be called at most once.
  void Start();

  // Stops the updater running, cancelling any outstanding update manifest and
  // crx downloads. Does not cancel any in-progress installs.
  void Stop();

  // Posts a task to do an update check.  Does nothing if there is
  // already a pending task that has not yet run.
  void CheckSoon();

  // Starts an update check for the specified extension soon.
  void CheckExtensionSoon(const std::string& extension_id,
                          const FinishedCallback& callback);

  // Starts an update check right now, instead of waiting for the next
  // regularly scheduled check or a pending check from CheckSoon().
  void CheckNow(const CheckParams& params);

  // Returns true iff CheckSoon() has been called but the update check
  // hasn't been performed yet.  This is used mostly by tests; calling
  // code should just call CheckSoon().
  bool WillCheckSoon() const;

  // Overrides the extension cache with |extension_cache| for testing.
  void SetExtensionCacheForTesting(ExtensionCache* extension_cache);

  // Stop the timer to prevent scheduled updates for testing.
  void StopTimerForTesting();

 private:
  friend class ExtensionUpdaterTest;
  friend class ExtensionUpdaterFileHandler;

  // FetchedCRXFile holds information about a CRX file we fetched to disk,
  // but have not yet installed.
  struct FetchedCRXFile {
    FetchedCRXFile();
    FetchedCRXFile(const CRXFileInfo& file,
                   bool file_ownership_passed,
                   const std::set<int>& request_ids,
                   const InstallCallback& callback);
    FetchedCRXFile(const FetchedCRXFile& other);
    ~FetchedCRXFile();

    CRXFileInfo info;
    GURL download_url;
    bool file_ownership_passed;
    std::set<int> request_ids;
    InstallCallback callback;
  };

  struct InProgressCheck {
    InProgressCheck();
    ~InProgressCheck();

    bool install_immediately;
    bool awaiting_update_service;
    FinishedCallback callback;
    // The ids of extensions that have in-progress update checks.
    std::set<std::string> in_progress_ids_;

    DISALLOW_COPY_AND_ASSIGN(InProgressCheck);
  };

  // Ensure that we have a valid ExtensionDownloader instance referenced by
  // |downloader|.
  void EnsureDownloaderCreated();

  // Computes when to schedule the first update check.
  base::TimeDelta DetermineFirstCheckDelay();

  // Sets the timer to call TimerFired after roughly |target_delay| from now.
  // To help spread load evenly on servers, this method adds some random
  // jitter. It also saves the scheduled time so it can be reloaded on
  // browser restart.
  void ScheduleNextCheck(const base::TimeDelta& target_delay);

  // Add fetch records for extensions that are installed to the downloader,
  // ignoring |pending_ids| so the extension isn't fetched again.
  void AddToDownloader(const ExtensionSet* extensions,
                       const std::list<std::string>& pending_ids,
                       int request_id,
                       ManifestFetchData::FetchPriority fetch_priority,
                       ExtensionUpdateCheckParams* update_check_params);

  // BaseTimer::ReceiverMethod callback.
  void TimerFired();

  // Posted by CheckSoon().
  void DoCheckSoon();

  // Implementation of ExtensionDownloaderDelegate.
  void OnExtensionDownloadFailed(const std::string& id,
                                 Error error,
                                 const PingResult& ping,
                                 const std::set<int>& request_ids) override;
  void OnExtensionDownloadFinished(const CRXFileInfo& file,
                                   bool file_ownership_passed,
                                   const GURL& download_url,
                                   const std::string& version,
                                   const PingResult& ping,
                                   const std::set<int>& request_id,
                                   const InstallCallback& callback) override;
  bool GetPingDataForExtension(const std::string& id,
                               ManifestFetchData::PingData* ping_data) override;
  std::string GetUpdateUrlData(const std::string& id) override;
  bool IsExtensionPending(const std::string& id) override;
  bool GetExtensionExistingVersion(const std::string& id,
                                   std::string* version) override;

  void UpdatePingData(const std::string& id, const PingResult& ping_result);

  // Starts installing a crx file that has been fetched but not installed yet.
  void MaybeInstallCRXFile();

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Send a notification that update checks are starting.
  void NotifyStarted();

  // Send a notification if we're finished updating.
  void NotifyIfFinished(int request_id);

  // |udpate_service_| will execute this function on finish.
  void OnUpdateServiceFinished(int request_id);

  void ExtensionCheckFinished(const std::string& extension_id,
                              const FinishedCallback& callback);

  // Whether Start() has been called but not Stop().
  bool alive_;

  // Pointer back to the service that owns this ExtensionUpdater.
  ExtensionServiceInterface* service_;

  // A closure passed into the ExtensionUpdater to teach it how to construct
  // new ExtensionDownloader instances.
  const ExtensionDownloader::Factory downloader_factory_;

  // Fetches the crx files for the extensions that have an available update.
  std::unique_ptr<ExtensionDownloader> downloader_;

  // Update service is responsible for updating Webstore extensions.
  // Note that |UpdateService| is a KeyedService class, which can only be
  // created through a |KeyedServiceFactory| singleton, thus |update_service_|
  // will be freed by the same factory singleton before the browser is
  // shutdown.
  UpdateService* update_service_;

  base::OneShotTimer timer_;
  int frequency_seconds_;
  bool will_check_soon_;

  ExtensionPrefs* extension_prefs_;
  PrefService* prefs_;
  Profile* profile_;

  std::map<int, InProgressCheck> requests_in_progress_;
  int next_request_id_;

  // Observes CRX installs we initiate.
  content::NotificationRegistrar registrar_;

  // True when a CrxInstaller is doing an install.  Used in MaybeUpdateCrxFile()
  // to keep more than one install from running at once.
  bool crx_install_is_running_;

  // Fetched CRX files waiting to be installed.
  std::stack<FetchedCRXFile> fetched_crx_files_;
  FetchedCRXFile current_crx_file_;

  ExtensionCache* extension_cache_;

  base::WeakPtrFactory<ExtensionUpdater> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUpdater);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_
