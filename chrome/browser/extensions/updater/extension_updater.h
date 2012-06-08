// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_
#pragma once

#include <list>
#include <stack>
#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/extensions/updater/extension_downloader_delegate.h"
#include "chrome/browser/extensions/updater/manifest_fetch_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class ExtensionPrefs;
class ExtensionServiceInterface;
class ExtensionSet;
class PrefService;
class Profile;

namespace extensions {

class ExtensionDownloader;
class ExtensionUpdaterTest;

// A class for doing auto-updates of installed Extensions. Used like this:
//
// ExtensionUpdater* updater = new ExtensionUpdater(my_extensions_service,
//                                                  extension_prefs,
//                                                  pref_service,
//                                                  profile,
//                                                  update_frequency_secs);
// updater->Start();
// ....
// updater->Stop();
class ExtensionUpdater : public ExtensionDownloaderDelegate,
                         public content::NotificationObserver {
 public:
  // Holds a pointer to the passed |service|, using it for querying installed
  // extensions and installing updated ones. The |frequency_seconds| parameter
  // controls how often update checks are scheduled.
  ExtensionUpdater(ExtensionServiceInterface* service,
                   ExtensionPrefs* extension_prefs,
                   PrefService* prefs,
                   Profile* profile,
                   int frequency_seconds);

  virtual ~ExtensionUpdater();

  // Starts the updater running.  Should be called at most once.
  void Start();

  // Stops the updater running, cancelling any outstanding update manifest and
  // crx downloads. Does not cancel any in-progress installs.
  void Stop();

  // Posts a task to do an update check.  Does nothing if there is
  // already a pending task that has not yet run.
  void CheckSoon();

  // Starts an update check right now, instead of waiting for the next
  // regularly scheduled check or a pending check from CheckSoon().
  void CheckNow();

  // Set blacklist checks on or off.
  void set_blacklist_checks_enabled(bool enabled) {
    blacklist_checks_enabled_ = enabled;
  }

  // Returns true iff CheckSoon() has been called but the update check
  // hasn't been performed yet.  This is used mostly by tests; calling
  // code should just call CheckSoon().
  bool WillCheckSoon() const;

 private:
  friend class ExtensionUpdaterTest;
  friend class ExtensionUpdaterFileHandler;

  // FetchedCRXFile holds information about a CRX file we fetched to disk,
  // but have not yet installed.
  struct FetchedCRXFile {
    FetchedCRXFile();
    FetchedCRXFile(const std::string& id,
                   const FilePath& path,
                   const GURL& download_url);
    ~FetchedCRXFile();

    std::string id;
    FilePath path;
    GURL download_url;
  };

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
                       const std::list<std::string>& pending_ids);

  // BaseTimer::ReceiverMethod callback.
  void TimerFired();

  // Posted by CheckSoon().
  void DoCheckSoon();

  // Implenentation of ExtensionDownloaderDelegate.
  virtual void OnExtensionDownloadFailed(const std::string& id,
                                         Error error,
                                         const PingResult& ping) OVERRIDE;

  virtual void OnExtensionDownloadFinished(const std::string& id,
                                           const FilePath& path,
                                           const GURL& download_url,
                                           const std::string& version,
                                           const PingResult& ping) OVERRIDE;

  virtual void OnBlacklistDownloadFinished(const std::string& data,
                                           const std::string& package_hash,
                                           const std::string& version,
                                           const PingResult& ping) OVERRIDE;
  virtual bool GetPingDataForExtension(
      const std::string& id,
      ManifestFetchData::PingData* ping_data) OVERRIDE;

  virtual std::string GetUpdateUrlData(const std::string& id) OVERRIDE;

  virtual bool IsExtensionPending(const std::string& id) OVERRIDE;

  virtual bool GetExtensionExistingVersion(const std::string& id,
                                           std::string* version) OVERRIDE;

  void UpdatePingData(const std::string& id, const PingResult& ping_result);

  // Starts installing a crx file that has been fetched but not installed yet.
  void MaybeInstallCRXFile();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Send a notification that update checks are starting.
  void NotifyStarted();

  // Send a notification if we're finished updating.
  void NotifyIfFinished();

  // Whether Start() has been called but not Stop().
  bool alive_;

  base::WeakPtrFactory<ExtensionUpdater> weak_ptr_factory_;

  // Pointer back to the service that owns this ExtensionUpdater.
  ExtensionServiceInterface* service_;

  // Fetches the crx files for the extensions that have an available update.
  scoped_ptr<ExtensionDownloader> downloader_;

  base::OneShotTimer<ExtensionUpdater> timer_;
  int frequency_seconds_;
  bool will_check_soon_;

  ExtensionPrefs* extension_prefs_;
  PrefService* prefs_;
  Profile* profile_;
  bool blacklist_checks_enabled_;

  // The ids of extensions that have in-progress update checks.
  std::list<std::string> in_progress_ids_;

  // Observes CRX installs we initiate.
  content::NotificationRegistrar registrar_;

  // True when a CrxInstaller is doing an install.  Used in MaybeUpdateCrxFile()
  // to keep more than one install from running at once.
  bool crx_install_is_running_;

  // Fetched CRX files waiting to be installed.
  std::stack<FetchedCRXFile> fetched_crx_files_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUpdater);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_H_
