// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_

#include <list>
#include <map>
#include <set>
#include <stack>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/updater/extension_downloader.h"
#include "chrome/browser/extensions/updater/extension_downloader_delegate.h"
#include "chrome/browser/extensions/updater/manifest_fetch_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"
#include "url/gurl.h"

class ExtensionServiceInterface;
class PrefService;
class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionCache;
class ExtensionPrefs;
class ExtensionRegistry;
class ExtensionSet;
class ExtensionUpdaterTest;

// A class for doing auto-updates of installed Extensions. Used like this:
//
// ExtensionUpdater* updater = new ExtensionUpdater(my_extensions_service,
//                                                  extension_prefs,
//                                                  pref_service,
//                                                  profile,
//                                                  update_frequency_secs,
//                                                  downloader_factory);
// updater->Start();
// ....
// updater->Stop();
class ExtensionUpdater : public ExtensionDownloaderDelegate,
                         public ExtensionRegistryObserver,
                         public content::NotificationObserver {
 public:
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

  virtual ~ExtensionUpdater();

  // Starts the updater running.  Should be called at most once.
  void Start();

  // Stops the updater running, cancelling any outstanding update manifest and
  // crx downloads. Does not cancel any in-progress installs.
  void Stop();

  // Posts a task to do an update check.  Does nothing if there is
  // already a pending task that has not yet run.
  void CheckSoon();

  // Starts an update check for the specified extension soon. If a check
  // is already running, or finished too recently without an update being
  // installed, this method returns false and the check won't be scheduled.
  bool CheckExtensionSoon(const std::string& extension_id,
                          const FinishedCallback& callback);

  // Starts an update check right now, instead of waiting for the next
  // regularly scheduled check or a pending check from CheckSoon().
  void CheckNow(const CheckParams& params);

  // Returns true iff CheckSoon() has been called but the update check
  // hasn't been performed yet.  This is used mostly by tests; calling
  // code should just call CheckSoon().
  bool WillCheckSoon() const;

  // Changes the params that are used for the automatic periodic update checks,
  // as well as for explicit calls to CheckSoon.
  void set_default_check_params(const CheckParams& params) {
    default_params_ = params;
  }

 private:
  friend class ExtensionUpdaterTest;
  friend class ExtensionUpdaterFileHandler;

  // FetchedCRXFile holds information about a CRX file we fetched to disk,
  // but have not yet installed.
  struct FetchedCRXFile {
    FetchedCRXFile();
    FetchedCRXFile(const std::string& id,
                   const base::FilePath& path,
                   bool file_ownership_passed,
                   const std::set<int>& request_ids);
    ~FetchedCRXFile();

    std::string extension_id;
    base::FilePath path;
    bool file_ownership_passed;
    std::set<int> request_ids;
  };

  struct InProgressCheck {
    InProgressCheck();
    ~InProgressCheck();

    bool install_immediately;
    FinishedCallback callback;
    // The ids of extensions that have in-progress update checks.
    std::list<std::string> in_progress_ids_;
  };

  struct ThrottleInfo;

  // Callback used to continue CheckNow after determining which extensions
  // should be force-updated.
  void OnForcedUpdatesDetermined(const CheckParams& params,
                                 const std::set<std::string>& forced_updates);

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
                       int request_id);

  // BaseTimer::ReceiverMethod callback.
  void TimerFired();

  // Posted by CheckSoon().
  void DoCheckSoon();

  // Implementation of ExtensionDownloaderDelegate.
  virtual void OnExtensionDownloadFailed(
      const std::string& id,
      Error error,
      const PingResult& ping,
      const std::set<int>& request_ids) OVERRIDE;
  virtual void OnExtensionDownloadFinished(
      const std::string& id,
      const base::FilePath& path,
      bool file_ownership_passed,
      const GURL& download_url,
      const std::string& version,
      const PingResult& ping,
      const std::set<int>& request_id) OVERRIDE;
  virtual bool GetPingDataForExtension(
      const std::string& id,
      ManifestFetchData::PingData* ping_data) OVERRIDE;
  virtual std::string GetUpdateUrlData(const std::string& id) OVERRIDE;
  virtual bool IsExtensionPending(const std::string& id) OVERRIDE;
  virtual bool GetExtensionExistingVersion(const std::string& id,
                                           std::string* version) OVERRIDE;
  virtual bool ShouldForceUpdate(const std::string& extension_id,
                                 std::string* source) OVERRIDE;

  void UpdatePingData(const std::string& id, const PingResult& ping_result);

  // Starts installing a crx file that has been fetched but not installed yet.
  void MaybeInstallCRXFile();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Implementation of ExtensionRegistryObserver.
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) OVERRIDE;

  // Send a notification that update checks are starting.
  void NotifyStarted();

  // Send a notification if we're finished updating.
  void NotifyIfFinished(int request_id);

  void ExtensionCheckFinished(const std::string& extension_id,
                              const FinishedCallback& callback);

  // Whether Start() has been called but not Stop().
  bool alive_;

  base::WeakPtrFactory<ExtensionUpdater> weak_ptr_factory_;

  // Pointer back to the service that owns this ExtensionUpdater.
  ExtensionServiceInterface* service_;

  // A closure passed into the ExtensionUpdater to teach it how to construct
  // new ExtensionDownloader instances.
  const ExtensionDownloader::Factory downloader_factory_;

  // Fetches the crx files for the extensions that have an available update.
  scoped_ptr<ExtensionDownloader> downloader_;

  base::OneShotTimer<ExtensionUpdater> timer_;
  int frequency_seconds_;
  bool will_check_soon_;

  ExtensionPrefs* extension_prefs_;
  PrefService* prefs_;
  Profile* profile_;

  std::map<int, InProgressCheck> requests_in_progress_;
  int next_request_id_;

  // Observes CRX installs we initiate.
  content::NotificationRegistrar registrar_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  // True when a CrxInstaller is doing an install.  Used in MaybeUpdateCrxFile()
  // to keep more than one install from running at once.
  bool crx_install_is_running_;

  // Fetched CRX files waiting to be installed.
  std::stack<FetchedCRXFile> fetched_crx_files_;
  FetchedCRXFile current_crx_file_;

  CheckParams default_params_;

  ExtensionCache* extension_cache_;

  // Keeps track of when an extension tried to update itself, so we can throttle
  // checks to prevent too many requests from being made.
  std::map<std::string, ThrottleInfo> throttle_info_;

  // Keeps track of extensions (by ID) whose update should be forced during the
  // next update check.
  std::set<std::string> forced_updates_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUpdater);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_
