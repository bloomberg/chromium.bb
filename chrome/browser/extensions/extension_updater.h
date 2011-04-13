// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UPDATER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UPDATER_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/update_manifest.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

class Extension;
class ExtensionPrefs;
class ExtensionUpdaterTest;
class ExtensionUpdaterFileHandler;
class PrefService;
class Profile;
class SafeManifestParser;

// To save on server resources we can request updates for multiple extensions
// in one manifest check. This class helps us keep track of the id's for a
// given fetch, building up the actual URL, and what if anything to include
// in the ping parameter.
class ManifestFetchData {
 public:
  static const int kNeverPinged = -1;

  // Each ping type is sent at most once per day.
  enum PingType {
    // Used for counting total installs of an extension/app/theme.
    ROLLCALL,

    // Used for counting number of active users of an app, where "active" means
    // the app was launched at least once since the last active ping.
    ACTIVE
  };

  struct PingData {
    // The number of days it's been since our last rollcall or active ping,
    // respectively. These are calculated based on the start of day from the
    // server's perspective.
    int rollcall_days;
    int active_days;

    PingData() : rollcall_days(0), active_days(0) {}
    PingData(int rollcall, int active)
        : rollcall_days(rollcall), active_days(active) {}
  };

  explicit ManifestFetchData(const GURL& update_url);
  ~ManifestFetchData();

  // Returns true if this extension information was successfully added. If the
  // return value is false it means the full_url would have become too long, and
  // this ManifestFetchData object remains unchanged.
  bool AddExtension(std::string id, std::string version,
                    const PingData& ping_data,
                    const std::string& update_url_data);

  const GURL& base_url() const { return base_url_; }
  const GURL& full_url() const { return full_url_; }
  int extension_count() { return extension_ids_.size(); }
  const std::set<std::string>& extension_ids() const { return extension_ids_; }

  // Returns true if the given id is included in this manifest fetch.
  bool Includes(const std::string& extension_id) const;

  // Returns true if a ping parameter for |type| was added to full_url for this
  // extension id.
  bool DidPing(std::string extension_id, PingType type) const;

 private:
  // The set of extension id's for this ManifestFetchData.
  std::set<std::string> extension_ids_;

  // The set of ping data we actually sent.
  std::map<std::string, PingData> pings_;

  // The base update url without any arguments added.
  GURL base_url_;

  // The base update url plus arguments indicating the id, version, etc.
  // information about each extension.
  GURL full_url_;

  DISALLOW_COPY_AND_ASSIGN(ManifestFetchData);
};

// A class for building a set of ManifestFetchData objects from
// extensions and pending extensions.
class ManifestFetchesBuilder {
 public:
  ManifestFetchesBuilder(ExtensionServiceInterface* service,
                         ExtensionPrefs* prefs);
  ~ManifestFetchesBuilder();

  void AddExtension(const Extension& extension);

  void AddPendingExtension(const std::string& id,
                           const PendingExtensionInfo& info);

  // Adds all recorded stats taken so far to histogram counts.
  void ReportStats() const;

  // Caller takes ownership of the returned ManifestFetchData
  // objects.  Clears all recorded stats.
  std::vector<ManifestFetchData*> GetFetches();

 private:
  struct URLStats {
    URLStats()
        : no_url_count(0),
          google_url_count(0),
          other_url_count(0),
          extension_count(0),
          theme_count(0),
          app_count(0),
          pending_count(0) {}

    int no_url_count, google_url_count, other_url_count;
    int extension_count, theme_count, app_count, pending_count;
  };

  void AddExtensionData(Extension::Location location,
                        const std::string& id,
                        const Version& version,
                        Extension::Type extension_type,
                        GURL update_url,
                        const std::string& update_url_data);
  ExtensionServiceInterface* const service_;
  ExtensionPrefs* const prefs_;

  // List of data on fetches we're going to do. We limit the number of
  // extensions grouped together in one batch to avoid running into the limits
  // on the length of http GET requests, so there might be multiple
  // ManifestFetchData* objects with the same base_url.
  std::multimap<GURL, ManifestFetchData*> fetches_;

  URLStats url_stats_;

  DISALLOW_COPY_AND_ASSIGN(ManifestFetchesBuilder);
};

// A class for doing auto-updates of installed Extensions. Used like this:
//
// ExtensionUpdater* updater = new ExtensionUpdater(my_extensions_service,
//                                                  pref_service,
//                                                  update_frequency_secs);
// updater->Start();
// ....
// updater->Stop();
class ExtensionUpdater : public URLFetcher::Delegate {
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
  friend class SafeManifestParser;

  // We need to keep track of some information associated with a url
  // when doing a fetch.
  struct ExtensionFetch {
    ExtensionFetch();
    ExtensionFetch(const std::string& i, const GURL& u,
                   const std::string& h, const std::string& v);
    ~ExtensionFetch();

    std::string id;
    GURL url;
    std::string package_hash;
    std::string version;
  };

  // These are needed for unit testing, to help identify the correct mock
  // URLFetcher objects.
  static const int kManifestFetcherId = 1;
  static const int kExtensionFetcherId = 2;

  static const char* kBlacklistAppID;

  // Does common work from constructors.
  void Init();

  // Computes when to schedule the first update check.
  base::TimeDelta DetermineFirstCheckDelay();

  // URLFetcher::Delegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // These do the actual work when a URL fetch completes.
  virtual void OnManifestFetchComplete(const GURL& url,
                                       const net::URLRequestStatus& status,
                                       int response_code,
                                       const std::string& data);
  virtual void OnCRXFetchComplete(const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const std::string& data);

  // Called when a crx file has been written into a temp file, and is ready
  // to be installed.
  void OnCRXFileWritten(const std::string& id,
                        const FilePath& path,
                        const GURL& download_url);

  // Called when we encountered an error writing a crx file to a temp file.
  void OnCRXFileWriteError(const std::string& id);

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

  // Posted by CheckSoon().
  void DoCheckSoon();

  // Begins an update check. Takes ownership of |fetch_data|.
  void StartUpdateCheck(ManifestFetchData* fetch_data);

  // Begins (or queues up) download of an updated extension.
  void FetchUpdatedExtension(const std::string& id, const GURL& url,
    const std::string& hash, const std::string& version);

  // Once a manifest is parsed, this starts fetches of any relevant crx files.
  // If |results| is null, it means something went wrong when parsing it.
  void HandleManifestResults(const ManifestFetchData& fetch_data,
                             const UpdateManifest::Results* results);

  // Determines the version of an existing extension.
  // Returns true on success and false on failures.
  bool GetExistingVersion(const std::string& id, std::string* version);

  // Given a list of potential updates, returns the indices of the ones that are
  // applicable (are actually a new version, etc.) in |result|.
  std::vector<int> DetermineUpdates(const ManifestFetchData& fetch_data,
      const UpdateManifest::Results& possible_updates);

  // Send a notification that update checks are starting.
  void NotifyStarted();

  // Send a notification that an update was found for extension_id that we'll
  // attempt to download and install.
  void NotifyUpdateFound(const std::string& extension_id);

  // Send a notification if we're finished updating.
  void NotifyIfFinished();

  // Adds a set of ids to in_progress_ids_.
  void AddToInProgress(const std::set<std::string>& ids);

  // Removes a set of ids from in_progress_ids_.
  void RemoveFromInProgress(const std::set<std::string>& ids);

  // Whether Start() has been called but not Stop().
  bool alive_;

  base::WeakPtrFactory<ExtensionUpdater> weak_ptr_factory_;

  // Outstanding url fetch requests for manifests and updates.
  scoped_ptr<URLFetcher> manifest_fetcher_;
  scoped_ptr<URLFetcher> extension_fetcher_;

  // Pending manifests and extensions to be fetched when the appropriate fetcher
  // is available.
  std::deque<ManifestFetchData*> manifests_pending_;
  std::deque<ExtensionFetch> extensions_pending_;

  // The manifest currently being fetched (if any).
  scoped_ptr<ManifestFetchData> current_manifest_fetch_;

  // The extension currently being fetched (if any).
  ExtensionFetch current_extension_fetch_;

  // Pointer back to the service that owns this ExtensionUpdater.
  ExtensionServiceInterface* service_;

  base::OneShotTimer<ExtensionUpdater> timer_;
  int frequency_seconds_;

  ScopedRunnableMethodFactory<ExtensionUpdater> method_factory_;

  bool will_check_soon_;

  ExtensionPrefs* extension_prefs_;
  PrefService* prefs_;
  Profile* profile_;

  scoped_refptr<ExtensionUpdaterFileHandler> file_handler_;
  bool blacklist_checks_enabled_;

  // The ids of extensions that have in-progress update checks.
  std::set<std::string> in_progress_ids_;

  FRIEND_TEST(ExtensionUpdaterTest, TestStartUpdateCheckMemory);
  FRIEND_TEST(ExtensionUpdaterTest, TestAfterStopBehavior);

  DISALLOW_COPY_AND_ASSIGN(ExtensionUpdater);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UPDATER_H_
