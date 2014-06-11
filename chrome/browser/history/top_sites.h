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
#include "components/history/core/common/thumbnail_score.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"

class GURL;
class Profile;

namespace base {
class FilePath;
class RefCountedBytes;
class RefCountedMemory;
}

namespace history {

class TopSitesCache;

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

  // While testing the history system, we want to set the thumbnail to a piece
  // of static memory.
  virtual bool SetPageThumbnailToJPEGBytes(
      const GURL& url,
      const base::RefCountedMemory* memory,
      const ThumbnailScore& score) = 0;

  typedef base::Callback<void(const MostVisitedURLList&)>
      GetMostVisitedURLsCallback;

  // Returns a list of most visited URLs via a callback, if
  // |include_forced_urls| is false includes only non-forced URLs. This may be
  // invoked on any thread. NOTE: the callback is called immediately if we have
  // the data cached. If data is not available yet, callback will later be
  // posted to the thread called this function.
  virtual void GetMostVisitedURLs(
      const GetMostVisitedURLsCallback& callback,
      bool include_forced_urls) = 0;

  // Gets a thumbnail for a given page. Returns true iff we have the thumbnail.
  // This may be invoked on any thread.
  // If an exact thumbnail URL match fails, |prefix_match| specifies whether or
  // not to try harder by matching the query thumbnail URL as URL prefix (as
  // defined by UrlIsPrefix()).
  // As this method may be invoked on any thread the ref count needs to be
  // incremented before this method returns, so this takes a scoped_refptr*.
  virtual bool GetPageThumbnail(
      const GURL& url,
      bool prefix_match,
      scoped_refptr<base::RefCountedMemory>* bytes) = 0;

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

  // Follows the cached redirect chain to convert any URL to its
  // canonical version. If no redirect chain is known for the URL,
  // return it without modification.
  virtual const std::string& GetCanonicalURLString(const GURL& url) const = 0;

  // Returns true if the top sites list of non-forced URLs is full (i.e. we
  // already have the maximum number of non-forced top sites).  This function
  // also returns false if TopSites isn't loaded yet.
  virtual bool IsNonForcedFull() = 0;

  // Returns true if the top sites list of forced URLs is full (i.e. we already
  // have the maximum number of forced top sites).  This function also returns
  // false if TopSites isn't loaded yet.
  virtual bool IsForcedFull() = 0;

  virtual bool loaded() const = 0;

  // Returns the set of prepopulate pages.
  virtual MostVisitedURLList GetPrepopulatePages() = 0;

  // Adds or updates a |url| for which we should force the capture of a
  // thumbnail next time it's visited. If there is already a non-forced URL
  // matching this |url| this call has no effect. Indicate this URL was laste
  // forced at |time| so we can evict the older URLs when needed. Should be
  // called from the UI thread.
  virtual bool AddForcedURL(const GURL& url, const base::Time& time) = 0;

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
