// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_HANDLER_H_
#define CHROME_BROWSER_FAVICON_FAVICON_HANDLER_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/common/favicon_url.h"
#include "chrome/common/ref_counted_util.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

class FaviconHandlerDelegate;
class Profile;
class SkBitmap;

namespace base {
class RefCountedMemory;
}

namespace content {
class NavigationEntry;
}

// FaviconHandler works with FaviconTabHelper to fetch the specific type of
// favicon.
//
// FetchFavicon requests the favicon from the favicon service which in turn
// requests the favicon from the history database. At this point
// we only know the URL of the page, and not necessarily the url of the
// favicon. To ensure we handle reloading stale favicons as well as
// reloading a favicon on page reload we always request the favicon from
// history regardless of whether the NavigationEntry has a favicon.
//
// After the navigation two types of events are delivered (which is
// first depends upon who is faster): notification from the history
// db on our request for the favicon (OnFaviconDataForInitialURL),
// or a message from the renderer giving us the URL of the favicon for
// the page (SetFaviconURL).
// . If the history db has a valid up to date favicon for the page, we update
//   the NavigationEntry and use the favicon.
// . When we receive the favicon url if it matches that of the NavigationEntry
//   and the NavigationEntry's favicon is set, we do nothing (everything is
//   ok).
// . On the other hand if the database does not know the favicon for url, or
//   the favicon is out date, or the URL from the renderer does not match that
//   NavigationEntry we proceed to DownloadFaviconOrAskHistory. Before we
//   invoke DownloadFaviconOrAskHistory we wait until we've received both
//   the favicon url and the callback from history. We wait to ensure we
//   truly know both the favicon url and the state of the database.
//
// DownloadFaviconOrAskHistory does the following:
// . If we have a valid favicon, but it is expired we ask the renderer to
//   download the favicon.
// . Otherwise we ask the history database to update the mapping from
//   page url to favicon url and call us back with the favicon. Remember, it is
//   possible for the db to already have the favicon, just not the mapping
//   between page to favicon url. The callback for this is OnFaviconData.
//
// OnFaviconData either updates the favicon of the NavigationEntry (if the
// db knew about the favicon), or requests the renderer to download the
// favicon.
//
// When the renderer downloads favicons, it considers the entire list of
// favicon candidates and chooses the one that best matches the preferred size
// (or the first one if there is no preferred size). Once the matching favicon
// has been determined, SetFavicon is called which updates the favicon of the
// NavigationEntry and notifies the database to save the favicon.

class FaviconHandler {
 public:
  enum Type {
    FAVICON,
    TOUCH,
  };

  FaviconHandler(Profile* profile,
                 FaviconHandlerDelegate* delegate,
                 Type icon_type);
  virtual ~FaviconHandler();

  // Initiates loading the favicon for the specified url.
  void FetchFavicon(const GURL& url);

  // Initiates loading an image from given |image_url|. Returns a download id
  // for caller to track the request. When download completes, |callback| is
  // called with the three params: the download_id, a boolean flag to indicate
  // whether the download succeeds and a SkBitmap as the downloaded image.
  // Note that |image_size| is a hint for images with multiple sizes. The
  // downloaded image is not resized to the given image_size. If 0 is passed,
  // the first frame of the image is returned.
  int DownloadImage(const GURL& image_url,
                    int image_size,
                    history::IconType icon_type,
                    const FaviconTabHelper::ImageDownloadCallback& callback);

  // Message Handler.  Must be public, because also called from
  // PrerenderContents. Collects the |image_urls| list.
  void OnUpdateFaviconURL(int32 page_id,
                          const std::vector<FaviconURL>& candidates);

  // Processes the current image_irls_ entry, requesting the image from the
  // history / download service.
  void ProcessCurrentUrl();

  // Message handler for IconHostMsg_DidDownloadFavicon. Called when the icon
  // at |image_url| has been downloaded.
  // |bitmaps| is a list of all the frames of the icon at |image_url|.
  void OnDidDownloadFavicon(int id,
                            const GURL& image_url,
                            bool errored,
                            int requested_size,
                            const std::vector<SkBitmap>& bitmaps);

  // For testing.
  const std::deque<FaviconURL>& image_urls() const { return image_urls_; }

 protected:
  // These virtual methods make FaviconHandler testable and are overridden by
  // TestFaviconHandler.

  // Return the NavigationEntry for the active entry, or NULL if the active
  // entries URL does not match that of the URL last passed to FetchFavicon.
  virtual content::NavigationEntry* GetEntry();

  // Asks the render to download favicon, returns the request id.
  virtual int DownloadFavicon(const GURL& image_url, int image_size);

  // Ask the favicon from history
  virtual void UpdateFaviconMappingAndFetch(
      const GURL& page_url,
      const GURL& icon_url,
      history::IconType icon_type,
      CancelableRequestConsumerBase* consumer,
      const FaviconService::FaviconResultsCallback& callback);

  virtual void GetFavicon(
      const GURL& icon_url,
      history::IconType icon_type,
      CancelableRequestConsumerBase* consumer,
      const FaviconService::FaviconResultsCallback& callback);

  virtual void GetFaviconForURL(
      const GURL& page_url,
      int icon_types,
      CancelableRequestConsumerBase* consumer,
      const FaviconService::FaviconResultsCallback& callback);

  virtual void SetHistoryFavicons(
      const GURL& page_url,
      const GURL& icon_url,
      history::IconType icon_type,
      const gfx::Image& image);

  virtual FaviconService* GetFaviconService();

  // Returns true if the favicon should be saved.
  virtual bool ShouldSaveFavicon(const GURL& url);

 private:
  friend class TestFaviconHandler; // For testing

  struct DownloadRequest {
    DownloadRequest();
    ~DownloadRequest();

    DownloadRequest(const GURL& url,
                    const GURL& image_url,
                    const FaviconTabHelper::ImageDownloadCallback& callback,
                    history::IconType icon_type);

    GURL url;
    GURL image_url;
    FaviconTabHelper::ImageDownloadCallback callback;
    history::IconType icon_type;
  };

  struct FaviconCandidate {
    FaviconCandidate();
    ~FaviconCandidate();

    FaviconCandidate(const GURL& url,
                     const GURL& image_url,
                     const gfx::Image& image,
                     float score,
                     history::IconType icon_type);

    GURL url;
    GURL image_url;
    gfx::Image image;
    float score;
    history::IconType icon_type;
  };

  // See description above class for details.
  void OnFaviconDataForInitialURL(
      FaviconService::Handle handle,
      std::vector<history::FaviconBitmapResult> favicon_bitmap_results,
      history::IconURLSizesMap icon_url_sizes);

  // If the favicon has expired, asks the renderer to download the favicon.
  // Otherwise asks history to update the mapping between page url and icon
  // url with a callback to OnFaviconData when done.
  void DownloadFaviconOrAskHistory(const GURL& page_url,
                                   const GURL& icon_url,
                                   history::IconType icon_type);

  // See description above class for details.
  void OnFaviconData(
      FaviconService::Handle handle,
      std::vector<history::FaviconBitmapResult> favicon_bitmap_results,
      history::IconURLSizesMap icon_url_sizes);

  // Schedules a download for the specified entry. This adds the request to
  // download_requests_.
  int ScheduleDownload(const GURL& url,
                       const GURL& image_url,
                       int image_size,
                       history::IconType icon_type,
                       const FaviconTabHelper::ImageDownloadCallback& callback);

  // Updates |favicon_candidate_| and returns true if it is an exact match.
  bool UpdateFaviconCandidate(const GURL& url,
                              const GURL& image_url,
                              const gfx::Image& image,
                              float score,
                              history::IconType icon_type);

  // Sets the image data for the favicon.
  void SetFavicon(const GURL& url,
                  const GURL& icon_url,
                  const gfx::Image& image,
                  history::IconType icon_type);

  // Converts the FAVICON's image data to an SkBitmap and sets it on the
  // NavigationEntry.
  // If the WebContents has a delegate, it is notified of the new favicon
  // (INVALIDATE_FAVICON).
  void UpdateFavicon(content::NavigationEntry* entry,
      const std::vector<history::FaviconBitmapResult>& favicon_bitmap_results);
  void UpdateFavicon(content::NavigationEntry* entry, const gfx::Image* image);

  void FetchFaviconInternal();

  // Return the current candidate if any.
  FaviconURL* current_candidate() {
    return (image_urls_.size() > 0) ? &image_urls_[0] : NULL;
  }

  // Returns the preferred_icon_size according icon_types_, 0 means no
  // preference.
  int preferred_icon_size() {
    return icon_types_ == history::FAVICON ? gfx::kFaviconSize : 0;
  }

  // Used for history requests.
  CancelableRequestConsumer cancelable_consumer_;

  // URL of the page we're requesting the favicon for.
  GURL url_;

  // Whether we got the initial response for the favicon back from the renderer.
  // See "Favicon Details" in tab_contents.cc for more details.
  bool got_favicon_from_history_;

  // Whether the favicon is out of date or the favicon data in
  // |history_results_| is known to be incomplete. If true, it means history
  // knows about the favicon, but we need to download the favicon because the
  // icon has expired or the data in the database is incomplete.
  bool favicon_expired_or_incomplete_;

  // Requests to the renderer to download favicons.
  typedef std::map<int, DownloadRequest> DownloadRequests;
  DownloadRequests download_requests_;

  // The combination of the supported icon types.
  const int icon_types_;

  // The prioritized favicon candidates from the page back from the renderer.
  std::deque<FaviconURL> image_urls_;

  // The FaviconBitmapResults from history.
  std::vector<history::FaviconBitmapResult> history_results_;

  // The Profile associated with this handler.
  Profile* profile_;

  // This handler's delegate.
  FaviconHandlerDelegate* delegate_;  // weak

  // Current favicon candidate.
  FaviconCandidate favicon_candidate_;

  DISALLOW_COPY_AND_ASSIGN(FaviconHandler);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_HANDLER_H_
