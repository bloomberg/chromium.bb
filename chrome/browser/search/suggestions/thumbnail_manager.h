// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_THUMBNAIL_MANAGER_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_THUMBNAIL_MANAGER_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

class Profile;

namespace suggestions {

class SuggestionsProfile;

// A class used to fetch server thumbnails asynchronously.
class ThumbnailManager : public chrome::BitmapFetcherDelegate {
 public:
  explicit ThumbnailManager(Profile* profile);
  virtual ~ThumbnailManager();

  // Initializes the |thumbnail_map_| with the proper mapping from website URL
  // to thumbnail URL.
  void InitializeThumbnailMap(const SuggestionsProfile& suggestions);

  // Retrieves stored thumbnail for website |url| asynchronously. Calls
  // |callback| with Bitmap pointer if found, and NULL otherwise. Should be
  // called from the UI thread.
  void GetPageThumbnail(
      const GURL& url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(ThumbnailManagerTest, InitializeThumbnailMapTest);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailManagerBrowserTest, FetchThumbnails);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailManagerBrowserTest, FetchThumbnailsInvalid);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailManagerBrowserTest,
                           FetchThumbnailsMultiple);

  typedef std::vector<base::Callback<void(const GURL&, const SkBitmap*)> >
      CallbackVector;

  // State related to a thumbnail fetch (associated website url, fetcher,
  // pending callbacks).
  struct ThumbnailRequest {
    ThumbnailRequest();
    explicit ThumbnailRequest(chrome::BitmapFetcher* f);
    ~ThumbnailRequest();

    void swap(ThumbnailRequest* other) {
      std::swap(url, other->url);
      std::swap(callbacks, other->callbacks);
      std::swap(fetcher, other->fetcher);
    }

    GURL url;
    chrome::BitmapFetcher* fetcher;
    // Queue for pending callbacks, which may accumulate while the request is in
    // flight.
    CallbackVector callbacks;
  };

  typedef std::map<const GURL, ThumbnailRequest> ThumbnailRequestMap;

  // Inherited from BitmapFetcherDelegate. Runs on the UI thread.
  virtual void OnFetchComplete(const GURL thumbnail_url,
                               const SkBitmap* bitmap) OVERRIDE;

  // Looks up thumbnail for |url|. If found, writes the result to
  // |thumbnail_url| and returns true. Otherwise just returns false.
  bool GetThumbnailURL(const GURL& url, GURL* thumbnail_url);

  // Used for substituting the request context during testing.
  void set_request_context(net::URLRequestContextGetter* context) {
    url_request_context_ = context;
  }

  // Map from URL to thumbnail URL. Should be kept up to date when a new
  // SuggestionsProfile is available.
  std::map<GURL, GURL> thumbnail_map_;

  // Map from each thumbnail URL to the request information (associated website
  // url, fetcher, pending callbacks).
  ThumbnailRequestMap pending_requests_;

  net::URLRequestContextGetter* url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailManager);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_THUMBNAIL_MANAGER_H_
