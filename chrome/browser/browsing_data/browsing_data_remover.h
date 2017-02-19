// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_

#include <memory>
#include "base/callback_forward.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_remover_delegate.h"

namespace content {
class BrowsingDataFilterBuilder;
}

////////////////////////////////////////////////////////////////////////////////
// BrowsingDataRemover is responsible for removing data related to browsing:
// visits in url database, downloads, cookies ...
//
//  USAGE:
//
//  0. Instantiation.
//
//       BrowsingDataRemover* remover =
//           BrowsingDataRemoverFactory::GetForBrowserContext(browser_context);
//
//  1. No observer.
//
//       remover->Remove(base::Time(), base::Time::Max(), REMOVE_COOKIES, ALL);
//
//  2. Using an observer to report when one's own removal task is finished.
//
//       class CookiesDeleter : public BrowsingDataRemover::Observer {
//         CookiesDeleter() { remover->AddObserver(this); }
//         ~CookiesDeleter() { remover->RemoveObserver(this); }
//
//         void DeleteCookies() {
//           remover->RemoveAndReply(base::Time(), base::Time::Max(),
//                                   REMOVE_COOKIES, ALL, this);
//         }
//
//         void OnBrowsingDataRemoverDone() {
//           LOG(INFO) << "Cookies were deleted.";
//         }
//       }
//
////////////////////////////////////////////////////////////////////////////////
//
// TODO(crbug.com/668114): BrowsingDataRemover does not currently support plugin
// data deletion. Use PluginDataRemover instead.
class BrowsingDataRemover {
 public:
  // Mask used for Remove.
  enum RemoveDataMask {
    REMOVE_APPCACHE = 1 << 0,
    REMOVE_CACHE = 1 << 1,
    REMOVE_COOKIES = 1 << 2,
    REMOVE_DOWNLOADS = 1 << 3,
    REMOVE_FILE_SYSTEMS = 1 << 4,
    REMOVE_FORM_DATA = 1 << 5,
    // In addition to visits, REMOVE_HISTORY removes keywords, last session and
    // passwords UI statistics.
    REMOVE_HISTORY = 1 << 6,
    REMOVE_INDEXEDDB = 1 << 7,
    REMOVE_LOCAL_STORAGE = 1 << 8,
    REMOVE_PLUGIN_DATA = 1 << 9,
    REMOVE_PASSWORDS = 1 << 10,
    REMOVE_WEBSQL = 1 << 11,
    REMOVE_CHANNEL_IDS = 1 << 12,
    REMOVE_MEDIA_LICENSES = 1 << 13,
    REMOVE_SERVICE_WORKERS = 1 << 14,
    REMOVE_SITE_USAGE_DATA = 1 << 15,
    // REMOVE_NOCHECKS intentionally does not check if the browser context is
    // prohibited from deleting history or downloads.
    REMOVE_NOCHECKS = 1 << 16,
    REMOVE_CACHE_STORAGE = 1 << 17,
#if defined(OS_ANDROID)
    REMOVE_WEBAPP_DATA = 1 << 18,
#endif
    REMOVE_DURABLE_PERMISSION = 1 << 19,
    REMOVE_EXTERNAL_PROTOCOL_DATA = 1 << 20,

    // The following flag is used only in tests. In normal usage, hosted app
    // data is controlled by the REMOVE_COOKIES flag, applied to the
    // protected-web origin.
    REMOVE_HOSTED_APP_DATA_TESTONLY = 1 << 31,

    // "Site data" includes cookies, appcache, file systems, indexedDBs, local
    // storage, webSQL, service workers, cache storage, plugin data, web app
    // data (on Android) and statistics about passwords.
    REMOVE_SITE_DATA = REMOVE_APPCACHE | REMOVE_COOKIES | REMOVE_FILE_SYSTEMS |
                       REMOVE_INDEXEDDB |
                       REMOVE_LOCAL_STORAGE |
                       REMOVE_PLUGIN_DATA |
                       REMOVE_SERVICE_WORKERS |
                       REMOVE_CACHE_STORAGE |
                       REMOVE_WEBSQL |
                       REMOVE_CHANNEL_IDS |
#if defined(OS_ANDROID)
                       REMOVE_WEBAPP_DATA |
#endif
                       REMOVE_SITE_USAGE_DATA |
                       REMOVE_DURABLE_PERMISSION |
                       REMOVE_EXTERNAL_PROTOCOL_DATA,

    // Datatypes protected by Important Sites.
    IMPORTANT_SITES_DATATYPES = REMOVE_SITE_DATA | REMOVE_CACHE,

    // Datatypes that can be deleted partially per URL / origin / domain,
    // whichever makes sense.
    FILTERABLE_DATATYPES = REMOVE_SITE_DATA | REMOVE_CACHE | REMOVE_DOWNLOADS,

    // Includes all the available remove options. Meant to be used by clients
    // that wish to wipe as much data as possible from a Profile, to make it
    // look like a new Profile.
    REMOVE_ALL = REMOVE_SITE_DATA | REMOVE_CACHE | REMOVE_DOWNLOADS |
                 REMOVE_FORM_DATA |
                 REMOVE_HISTORY |
                 REMOVE_PASSWORDS |
                 REMOVE_MEDIA_LICENSES,

    // Includes all available remove options. Meant to be used when the Profile
    // is scheduled to be deleted, and all possible data should be wiped from
    // disk as soon as possible.
    REMOVE_WIPE_PROFILE = REMOVE_ALL | REMOVE_NOCHECKS,
  };

  // Important sites protect a small set of sites from the deletion of certain
  // datatypes. Therefore, those datatypes must be filterable by
  // url/origin/domain.
  static_assert(0 == (IMPORTANT_SITES_DATATYPES & ~FILTERABLE_DATATYPES),
                "All important sites datatypes must be filterable.");

  // A helper enum to report the deletion of cookies and/or cache. Do not
  // reorder the entries, as this enum is passed to UMA.
  enum CookieOrCacheDeletionChoice {
    NEITHER_COOKIES_NOR_CACHE,
    ONLY_COOKIES,
    ONLY_CACHE,
    BOTH_COOKIES_AND_CACHE,
    MAX_CHOICE_VALUE
  };

  // Observer is notified when its own removal task is done.
  class Observer {
   public:
    // Called when a removal task is finished. Note that every removal task can
    // only have one observer attached to it, and only that one is called.
    virtual void OnBrowsingDataRemoverDone() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Called by the embedder to provide the delegate that will take care of
  // deleting embedder-specific data.
  virtual void SetEmbedderDelegate(
      std::unique_ptr<BrowsingDataRemoverDelegate> embedder_delegate) = 0;
  virtual BrowsingDataRemoverDelegate* GetEmbedderDelegate() const = 0;

  // Removes browsing data within the given |time_range|, with datatypes being
  // specified by |remove_mask| and origin types by |origin_type_mask|.
  virtual void Remove(const base::Time& delete_begin,
                      const base::Time& delete_end,
                      int remove_mask,
                      int origin_type_mask) = 0;

  // A version of the above that in addition informs the |observer| when the
  // removal task is finished.
  virtual void RemoveAndReply(const base::Time& delete_begin,
                              const base::Time& delete_end,
                              int remove_mask,
                              int origin_type_mask,
                              Observer* observer) = 0;

  // Like Remove(), but in case of URL-keyed only removes data whose URL match
  // |filter_builder| (e.g. are on certain origin or domain).
  // RemoveWithFilter() currently only works with FILTERABLE_DATATYPES.
  virtual void RemoveWithFilter(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder) = 0;

  // A version of the above that in addition informs the |observer| when the
  // removal task is finished.
  virtual void RemoveWithFilterAndReply(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
      Observer* observer) = 0;

  // Observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Parameters of the last call are exposed to be used by tests. Removal and
  // origin type masks equal to -1 mean that no removal has ever been executed.
  // TODO(msramek): If other consumers than tests are interested in this,
  // consider returning them in OnBrowsingDataRemoverDone() callback. If not,
  // consider simplifying this interface by removing these methods and changing
  // the tests to record the parameters using GMock instead.
  virtual const base::Time& GetLastUsedBeginTime() = 0;
  virtual const base::Time& GetLastUsedEndTime() = 0;
  virtual int GetLastUsedRemovalMask() = 0;
  virtual int GetLastUsedOriginTypeMask() = 0;
};

#endif // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
