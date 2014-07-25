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
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace suggestions {

class SuggestionsProfile;
class ThumbnailData;

// A class used to fetch server thumbnails asynchronously and manage the caching
// layer (both in memory and on disk).
class ThumbnailManager : public ImageManager,
                         public chrome::BitmapFetcherDelegate {
 public:
  typedef std::vector<ThumbnailData> ThumbnailVector;

  ThumbnailManager(
      net::URLRequestContextGetter* url_request_context,
      scoped_ptr<leveldb_proto::ProtoDatabase<ThumbnailData> > database,
      const base::FilePath& database_dir);
  virtual ~ThumbnailManager();

  // Overrides from ImageManager.
  virtual void Initialize(const SuggestionsProfile& suggestions) OVERRIDE;
  // Should be called from the UI thread.
  virtual void GetImageForURL(
      const GURL& url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback) OVERRIDE;

 private:
  friend class MockThumbnailManager;
  friend class ThumbnailManagerBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ThumbnailManagerTest, InitializeTest);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailManagerBrowserTest,
                           GetImageForURLNetworkCacheHit);
  FRIEND_TEST_ALL_PREFIXES(ThumbnailManagerBrowserTest,
                           GetImageForURLNetworkCacheNotInitialized);

  // Used for testing.
  ThumbnailManager();

  typedef std::vector<base::Callback<void(const GURL&, const SkBitmap*)> >
      CallbackVector;
  typedef base::hash_map<std::string, SkBitmap> ThumbnailMap;

  // State related to a thumbnail fetch (associated website url, thumbnail_url,
  // fetcher, pending callbacks).
  struct ThumbnailRequest {
    ThumbnailRequest();
    explicit ThumbnailRequest(chrome::BitmapFetcher* f);
    ~ThumbnailRequest();

    void swap(ThumbnailRequest* other) {
      std::swap(url, other->url);
      std::swap(thumbnail_url, other->thumbnail_url);
      std::swap(callbacks, other->callbacks);
      std::swap(fetcher, other->fetcher);
    }

    GURL url;
    GURL thumbnail_url;
    chrome::BitmapFetcher* fetcher;
    // Queue for pending callbacks, which may accumulate while the request is in
    // flight.
    CallbackVector callbacks;
  };

  typedef std::map<const GURL, ThumbnailRequest> ThumbnailRequestMap;

  // Looks up thumbnail for |url|. If found, writes the result to
  // |thumbnail_url| and returns true. Otherwise just returns false.
  bool GetThumbnailURL(const GURL& url, GURL* thumbnail_url);

  void QueueCacheRequest(
      const GURL& url, const GURL& thumbnail_url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

  void ServeFromCacheOrNetwork(
      const GURL& url, const GURL& thumbnail_url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

  // Will return false if no bitmap was found corresponding to |url|, else
  // return true and call |callback| with the found bitmap.
  bool ServeFromCache(
      const GURL& url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

  // Returns null if the |url| had no entry in the cache.
  SkBitmap* GetBitmapFromCache(const GURL& url);

  void StartOrQueueNetworkRequest(
      const GURL& url, const GURL& thumbnail_url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

  // Inherited from BitmapFetcherDelegate. Runs on the UI thread.
  virtual void OnFetchComplete(const GURL thumbnail_url,
                               const SkBitmap* bitmap) OVERRIDE;

  // Save the thumbnail bitmap in the cache and in the database.
  void SaveThumbnail(const GURL& url, const SkBitmap& bitmap);

  // Database callback methods.
  // Will initiate loading the entries.
  void OnDatabaseInit(bool success);
  // Will transfer the loaded |entries| in memory (|thumbnail_map_|).
  void OnDatabaseLoad(bool success, scoped_ptr<ThumbnailVector> entries);
  void OnDatabaseSave(bool success);

  // Take entries from the database and put them in the local cache.
  void LoadEntriesInCache(scoped_ptr<ThumbnailVector> entries);

  void ServePendingCacheRequests();

  // From SkBitmap to the vector of JPEG-encoded bytes, |dst|. Visible only for
  // testing.
  static bool EncodeThumbnail(const SkBitmap& bitmap,
                              std::vector<unsigned char>* dest);

  // Map from URL to thumbnail URL. Should be kept up to date when a new
  // SuggestionsProfile is available.
  std::map<GURL, GURL> thumbnail_url_map_;

  // Map from each thumbnail URL to the request information (associated website
  // url, fetcher, pending callbacks).
  ThumbnailRequestMap pending_net_requests_;

  // Map from website URL to request information, used for pending cache
  // requests while the database hasn't loaded.
  ThumbnailRequestMap pending_cache_requests_;

  // Holding the bitmaps in memory, keyed by website URL string.
  ThumbnailMap thumbnail_map_;

  net::URLRequestContextGetter* url_request_context_;

  scoped_ptr<leveldb_proto::ProtoDatabase<ThumbnailData> > database_;

  bool database_ready_;

  base::WeakPtrFactory<ThumbnailManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailManager);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_THUMBNAIL_MANAGER_H_
