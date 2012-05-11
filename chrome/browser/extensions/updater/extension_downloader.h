// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_DOWNLOADER_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_DOWNLOADER_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/updater/extension_downloader_delegate.h"
#include "chrome/browser/extensions/updater/manifest_fetch_data.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/update_manifest.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

class Version;

namespace net {
class URLRequestContextGetter;
class URLRequestStatus;
}

namespace extensions {

class ExtensionUpdaterTest;

// A class that checks for updates of a given list of extensions, and downloads
// the crx file when updates are found. It uses a |ExtensionDownloaderDelegate|
// that takes ownership of the downloaded crx files, and handles events during
// the update check.
class ExtensionDownloader : public content::URLFetcherDelegate {
 public:
  // |delegate| is stored as a raw pointer and must outlive the
  // ExtensionDownloader.
  ExtensionDownloader(ExtensionDownloaderDelegate* delegate,
                      net::URLRequestContextGetter* request_context);
  virtual ~ExtensionDownloader();

  // Adds |extension| to the list of extensions to check for updates.
  // Returns false if the |extension| can't be updated due to invalid details.
  // In that case, no callbacks will be performed on the |delegate_|.
  bool AddExtension(const Extension& extension);

  // Adds extension |id| to the list of extensions to check for updates.
  // Returns false if the |id| can't be updated due to invalid details.
  // In that case, no callbacks will be performed on the |delegate_|.
  bool AddPendingExtension(const std::string& id, const GURL& update_url);

  // Schedules a fetch of the manifest of all the extensions added with
  // AddExtension() and AddPendingExtension().
  void StartAllPending();

  // Schedules an update check of the blacklist.
  void StartBlacklistUpdate(const std::string& version,
                            const ManifestFetchData::PingData& ping_data);

  // These are needed for unit testing, to help identify the correct mock
  // URLFetcher objects.
  static const int kManifestFetcherId = 1;
  static const int kExtensionFetcherId = 2;

  // Update AppID for extension blacklist.
  static const char kBlacklistAppID[];

 private:
  friend class ExtensionUpdaterTest;

  // These counters are bumped as extensions are added to be fetched. They
  // are then recorded as UMA metrics when all the extensions have been added.
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

  // We need to keep track of some information associated with a url
  // when doing a fetch.
  struct ExtensionFetch {
    ExtensionFetch();
    ExtensionFetch(const std::string& id, const GURL& url,
                   const std::string& package_hash, const std::string& version);
    ~ExtensionFetch();

    std::string id;
    GURL url;
    std::string package_hash;
    std::string version;
  };

  // Helper for AddExtension() and AddPendingExtension().
  bool AddExtensionData(const std::string& id,
                        const Version& version,
                        Extension::Type extension_type,
                        GURL update_url,
                        const std::string& update_url_data);

  // Adds all recorded stats taken so far to histogram counts.
  void ReportStats() const;

  // Begins an update check. Takes ownership of |fetch_data|.
  void StartUpdateCheck(ManifestFetchData* fetch_data);

  // content::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Handles the result of a manifest fetch.
  void OnManifestFetchComplete(const GURL& url,
                               const net::URLRequestStatus& status,
                               int response_code,
                               const std::string& data);

  // Once a manifest is parsed, this starts fetches of any relevant crx files.
  // If |results| is null, it means something went wrong when parsing it.
  void HandleManifestResults(const ManifestFetchData& fetch_data,
                             const UpdateManifest::Results* results);

  // Given a list of potential updates, returns the indices of the ones that are
  // applicable (are actually a new version, etc.) in |result|.
  void DetermineUpdates(const ManifestFetchData& fetch_data,
                        const UpdateManifest::Results& possible_updates,
                        std::vector<int>* result);

  // Begins (or queues up) download of an updated extension.
  void FetchUpdatedExtension(const std::string& id,
                             const GURL& url,
                             const std::string& hash,
                             const std::string& version);

  // Handles the result of a crx fetch.
  void OnCRXFetchComplete(const net::URLFetcher* source,
                          const GURL& url,
                          const net::URLRequestStatus& status,
                          int response_code);

  // Invokes OnExtensionDownloadFailed() on the |delegate_| for each extension
  // in the set, with |error| as the reason for failure.
  void NotifyExtensionsDownloadFailed(const std::set<std::string>& id_set,
                                      ExtensionDownloaderDelegate::Error error);

  // Send a notification that an update was found for |id| that we'll
  // attempt to download.
  void NotifyUpdateFound(const std::string& id);

  // The delegate that receives the crx files downloaded by the
  // ExtensionDownloader, and that fills in optional ping and update url data.
  ExtensionDownloaderDelegate* delegate_;

  // The request context to use for the URLFetchers.
  net::URLRequestContextGetter* request_context_;

  // Used to create WeakPtrs to |this|.
  base::WeakPtrFactory<ExtensionDownloader> weak_ptr_factory_;

  // Collects UMA samples that are reported when ReportStats() is called.
  URLStats url_stats_;

  // List of data on fetches we're going to do. We limit the number of
  // extensions grouped together in one batch to avoid running into the limits
  // on the length of http GET requests, so there might be multiple
  // ManifestFetchData* objects with the same base_url.
  typedef std::map<GURL, std::vector<ManifestFetchData*> > FetchMap;
  FetchMap fetches_preparing_;

  // Outstanding url fetch requests for manifests and updates.
  scoped_ptr<content::URLFetcher> manifest_fetcher_;
  scoped_ptr<content::URLFetcher> extension_fetcher_;

  // Pending manifests and extensions to be fetched when the appropriate fetcher
  // is available.
  std::deque<ManifestFetchData*> manifests_pending_;
  std::deque<ExtensionFetch> extensions_pending_;

  // The manifest currently being fetched (if any).
  scoped_ptr<ManifestFetchData> current_manifest_fetch_;

  // The extension currently being fetched (if any).
  ExtensionFetch current_extension_fetch_;

  // Maps an extension-id to its PingResult data.
  std::map<std::string, ExtensionDownloaderDelegate::PingResult> ping_results_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDownloader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_DOWNLOADER_H_
