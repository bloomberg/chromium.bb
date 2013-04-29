// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/thumbnail_score.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"

class Profile;

namespace base {
class FilePath;
class RefCountedBytes;
class RefCountedMemory;
}

namespace history {

class TopSitesCache;
class TopSitesTest;

// Interface for TopSites, which stores the data for the top "most visited"
// sites. This includes a cache of the most visited data from history, as well
// as the corresponding thumbnails of those sites.
//
// Some methods should only be called from the UI thread (see method
// descriptions below). All others are assumed to be threadsafe.
class TopSites
    : public base::RefCountedThreadSafe<TopSites>,
      public content::NotificationObserver {
 public:
  TopSites() {}

  // Initializes TopSites.
  static TopSites* Create(Profile* profile, const base::FilePath& db_name);

  // Sets the given thumbnail for the given URL. Returns true if the thumbnail
  // was updated. False means either the URL wasn't known to us, or we felt
  // that our current thumbnail was superior to the given one. Should be called
  // from the UI thread.
  virtual bool SetPageThumbnail(const GURL& url,
                                const gfx::Image& thumbnail,
                                const ThumbnailScore& score) = 0;

  typedef base::Callback<void(const MostVisitedURLList&)>
      GetMostVisitedURLsCallback;
  typedef std::vector<GetMostVisitedURLsCallback> PendingCallbacks;

  // Returns a list of most visited URLs via a callback. This may be invoked on
  // any thread.
  // NOTE: the callback is called immediately if we have the data cached. If
  // data is not available yet, callback will later be posted to the thread
  // called this function.
  virtual void GetMostVisitedURLs(
      const GetMostVisitedURLsCallback& callback) = 0;

  // Get a thumbnail for a given page. Returns true iff we have the thumbnail.
  // This may be invoked on any thread.
  // As this method may be invoked on any thread the ref count needs to be
  // incremented before this method returns, so this takes a scoped_refptr*.
  virtual bool GetPageThumbnail(
      const GURL& url, scoped_refptr<base::RefCountedMemory>* bytes) = 0;

  // Get a thumbnail score for a given page. Returns true iff we have the
  // thumbnail score.  This may be invoked on any thread. The score will
  // be copied to |score|.
  virtual bool GetPageThumbnailScore(const GURL& url,
                                     ThumbnailScore* score) = 0;

  // Get a temporary thumbnail score for a given page. Returns true iff we
  // have the thumbnail score. Useful when checking if we should update a
  // thumbnail for a given page. The score will be copied to |score|.
  virtual bool GetTemporaryPageThumbnailScore(const GURL& url,
                                              ThumbnailScore* score) = 0;

  // Invoked from History if migration is needed. If this is invoked it will
  // be before HistoryLoaded is invoked. Should be called from the UI thread.
  virtual void MigrateFromHistory() = 0;

  // Invoked with data from migrating thumbnails out of history. Should be
  // called from the UI thread.
  virtual void FinishHistoryMigration(const ThumbnailMigration& data) = 0;

  // Invoked from history when it finishes loading. If MigrateFromHistory was
  // not invoked at this point then we load from the top sites service. Should
  // be called from the UI thread.
  virtual void HistoryLoaded() = 0;

  // Asks TopSites to refresh what it thinks the top sites are. This may do
  // nothing. Should be called from the UI thread.
  virtual void SyncWithHistory() = 0;

  // Blacklisted URLs

  // Returns true if there is at least one item in the blacklist.
  virtual bool HasBlacklistedItems() const = 0;

  // Add a URL to the blacklist. Should be called from the UI thread.
  virtual void AddBlacklistedURL(const GURL& url) = 0;

  // Removes a URL from the blacklist. Should be called from the UI thread.
  virtual void RemoveBlacklistedURL(const GURL& url) = 0;

  // Returns true if the URL is blacklisted. Should be called from the UI
  // thread.
  virtual bool IsBlacklisted(const GURL& url) = 0;

  // Clear the blacklist. Should be called from the UI thread.
  virtual void ClearBlacklistedURLs() = 0;

  // Shuts down top sites.
  virtual void Shutdown() = 0;

  // Query history service for the list of available thumbnails. Returns the
  // handle for the request, or NULL if a request could not be made.
  // Public only for testing purposes.
  virtual CancelableRequestProvider::Handle StartQueryForMostVisited() = 0;

  // Returns true if the given URL is known to the top sites service.
  // This function also returns false if TopSites isn't loaded yet.
  virtual bool IsKnownURL(const GURL& url) = 0;

  // Returns true if the top sites list is full (i.e. we already have the
  // maximum number of top sites).  This function also returns false if
  // TopSites isn't loaded yet.
  virtual bool IsFull() = 0;

  virtual bool loaded() const = 0;

  // Returns the set of prepopulate pages.
  virtual MostVisitedURLList GetPrepopulatePages() = 0;

  struct PrepopulatedPage {
    // The string resource for the url.
    int url_id;
    // The string resource for the page title.
    int title_id;
    // The raw data resource for the favicon.
    int favicon_id;
    // The raw data resource for the thumbnail.
    int thumbnail_id;
    // The best color to highlight the page (should roughly match favicon).
    SkColor color;
  };
 protected:
  virtual ~TopSites() {}

 private:
  friend class base::RefCountedThreadSafe<TopSites>;
};

#if defined(OS_ANDROID)
extern const TopSites::PrepopulatedPage kPrepopulatedPages[1];
#else
extern const TopSites::PrepopulatedPage kPrepopulatedPages[2];
#endif

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_H_
