// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UPDATER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UPDATER_H_

#include <deque>
#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/task.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/common/extensions/update_manifest.h"
#include "googleurl/src/gurl.h"

class Extension;
class ExtensionUpdaterTest;
class ExtensionUpdaterFileHandler;
class PrefService;

// A class for doing auto-updates of installed Extensions. Used like this:
//
// ExtensionUpdater* updater = new ExtensionUpdater(my_extensions_service,
//                                                  pref_service,
//                                                  update_frequency_secs);
// updater.Start();
// ....
// updater.Stop();
class ExtensionUpdater
    : public URLFetcher::Delegate,
      public base::RefCountedThreadSafe<ExtensionUpdater> {
 public:
  // Holds a pointer to the passed |service|, using it for querying installed
  // extensions and installing updated ones. The |frequency_seconds| parameter
  // controls how often update checks are scheduled.
  ExtensionUpdater(ExtensionUpdateService* service,
                   PrefService* prefs,
                   int frequency_seconds);

  // Starts the updater running.
  void Start();

  // Stops the updater running, cancelling any outstanding update manifest and
  // crx downloads. Does not cancel any in-progress installs.
  void Stop();

  // Starts an update check right now, instead of waiting for the next regularly
  // scheduled check.
  void CheckNow();

  // Set blacklist checks on or off.
  void set_blacklist_checks_enabled(bool enabled) {
    blacklist_checks_enabled_ = enabled;
  }

 private:
  friend class base::RefCountedThreadSafe<ExtensionUpdater>;
  friend class ExtensionUpdaterTest;
  friend class ExtensionUpdaterFileHandler;
  friend class SafeManifestParser;

  virtual ~ExtensionUpdater();

  // We need to keep track of some information associated with a url
  // when doing a fetch.
  struct ExtensionFetch {
    std::string id;
    GURL url;
    std::string package_hash;
    std::string version;
    ExtensionFetch() : id(""), url(), package_hash(""), version("") {}
    ExtensionFetch(const std::string& i, const GURL& u,
      const std::string& h, const std::string& v)
      : id(i), url(u), package_hash(h), version(v) {}
  };

  // These are needed for unit testing, to help identify the correct mock
  // URLFetcher objects.
  static const int kManifestFetcherId = 1;
  static const int kExtensionFetcherId = 2;

  static const char* kBlacklistUpdateUrl;
  static const char* kBlacklistAppID;

  // Does common work from constructors.
  void Init();

  // Computes when to schedule the first update check.
  base::TimeDelta DetermineFirstCheckDelay();

  // URLFetcher::Delegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // These do the actual work when a URL fetch completes.
  virtual void OnManifestFetchComplete(const GURL& url,
                                       const URLRequestStatus& status,
                                       int response_code,
                                       const std::string& data);
  virtual void OnCRXFetchComplete(const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const std::string& data);

  // Called when a crx file has been written into a temp file, and is ready
  // to be installed.
  void OnCRXFileWritten(const std::string& id, const FilePath& path);

  // Callback for when ExtensionsService::Install is finished.
  void OnExtensionInstallFinished(const FilePath& path, Extension* extension);

  // Verifies downloaded blacklist. Based on the blacklist, calls extension
  // service to unload blacklisted extensions and update pref.
  void ProcessBlacklist(const std::string& data);

  // Sets the timer to call TimerFired after roughly |target_delay| from now.
  // To help spread load evenly on servers, this method adds some random
  // jitter. It also saves the scheduled time so it can be reloaded on
  // browser restart.
  void ScheduleNextCheck(const base::TimeDelta& target_delay);

  // BaseTimer::ReceiverMethod callback.
  void TimerFired();

  // Begins an update check - called with url to fetch an update manifest.
  void StartUpdateCheck(const GURL& url);

  // Begins (or queues up) download of an updated extension.
  void FetchUpdatedExtension(const std::string& id, const GURL& url,
    const std::string& hash, const std::string& version);

  // Once a manifest is parsed, this starts fetches of any relevant crx files.
  void HandleManifestResults(const UpdateManifest::ResultList& results);

  // Determines the version of an existing extension.
  // Returns true on success and false on failures.
  bool GetExistingVersion(const std::string& id, std::string* version);

  // Given a list of potential updates, returns the indices of the ones that are
  // applicable (are actually a new version, etc.) in |result|.
  std::vector<int> DetermineUpdates(
      const std::vector<UpdateManifest::Result>& possible_updates);

  // Creates a blacklist update url.
  static GURL GetBlacklistUpdateUrl(const std::wstring& version);

  // Outstanding url fetch requests for manifests and updates.
  scoped_ptr<URLFetcher> manifest_fetcher_;
  scoped_ptr<URLFetcher> extension_fetcher_;

  // Pending manifests and extensions to be fetched when the appropriate fetcher
  // is available.
  std::deque<GURL> manifests_pending_;
  std::deque<ExtensionFetch> extensions_pending_;

  // The extension currently being fetched (if any).
  ExtensionFetch current_extension_fetch_;

  // Pointer back to the service that owns this ExtensionUpdater.
  ExtensionUpdateService* service_;

  base::OneShotTimer<ExtensionUpdater> timer_;
  int frequency_seconds_;

  PrefService* prefs_;

  scoped_refptr<ExtensionUpdaterFileHandler> file_handler_;
  bool blacklist_checks_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUpdater);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UPDATER_H_
