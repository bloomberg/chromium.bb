// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_MANAGER_IMPL_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_MANAGER_IMPL_H_

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

class ImageData;
class SuggestionsProfile;

// A class used to fetch server images asynchronously and manage the caching
// layer (both in memory and on disk).
class ImageManagerImpl : public ImageManager,
                         public chrome::BitmapFetcherDelegate {
 public:
  typedef std::vector<ImageData> ImageDataVector;

  ImageManagerImpl(
      net::URLRequestContextGetter* url_request_context,
      scoped_ptr<leveldb_proto::ProtoDatabase<ImageData> > database,
      const base::FilePath& database_dir);
  virtual ~ImageManagerImpl();

  // Overrides from ImageManager.
  virtual void Initialize(const SuggestionsProfile& suggestions) OVERRIDE;
  // Should be called from the UI thread.
  virtual void GetImageForURL(
      const GURL& url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback) OVERRIDE;

 private:
  friend class MockImageManagerImpl;
  friend class ImageManagerImplBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ImageManagerImplTest, InitializeTest);
  FRIEND_TEST_ALL_PREFIXES(ImageManagerImplBrowserTest,
                           GetImageForURLNetworkCacheHit);
  FRIEND_TEST_ALL_PREFIXES(ImageManagerImplBrowserTest,
                           GetImageForURLNetworkCacheNotInitialized);

  // Used for testing.
  ImageManagerImpl();

  typedef std::vector<base::Callback<void(const GURL&, const SkBitmap*)> >
      CallbackVector;
  typedef base::hash_map<std::string, SkBitmap> ImageMap;

  // State related to an image fetch (associated website url, image_url,
  // fetcher, pending callbacks).
  struct ImageRequest {
    ImageRequest();
    explicit ImageRequest(chrome::BitmapFetcher* f);
    ~ImageRequest();

    void swap(ImageRequest* other) {
      std::swap(url, other->url);
      std::swap(image_url, other->image_url);
      std::swap(callbacks, other->callbacks);
      std::swap(fetcher, other->fetcher);
    }

    GURL url;
    GURL image_url;
    chrome::BitmapFetcher* fetcher;
    // Queue for pending callbacks, which may accumulate while the request is in
    // flight.
    CallbackVector callbacks;
  };

  typedef std::map<const GURL, ImageRequest> ImageRequestMap;

  // Looks up image URL for |url|. If found, writes the result to |image_url|
  // and returns true. Otherwise just returns false.
  bool GetImageURL(const GURL& url, GURL* image_url);

  void QueueCacheRequest(
      const GURL& url, const GURL& image_url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

  void ServeFromCacheOrNetwork(
      const GURL& url, const GURL& image_url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

  // Will return false if no bitmap was found corresponding to |url|, else
  // return true and call |callback| with the found bitmap.
  bool ServeFromCache(
      const GURL& url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

  // Returns null if the |url| had no entry in the cache.
  SkBitmap* GetBitmapFromCache(const GURL& url);

  void StartOrQueueNetworkRequest(
      const GURL& url, const GURL& image_url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

  // Inherited from BitmapFetcherDelegate. Runs on the UI thread.
  virtual void OnFetchComplete(const GURL image_url,
                               const SkBitmap* bitmap) OVERRIDE;

  // Save the image bitmap in the cache and in the database.
  void SaveImage(const GURL& url, const SkBitmap& bitmap);

  // Database callback methods.
  // Will initiate loading the entries.
  void OnDatabaseInit(bool success);
  // Will transfer the loaded |entries| in memory (|image_map_|).
  void OnDatabaseLoad(bool success, scoped_ptr<ImageDataVector> entries);
  void OnDatabaseSave(bool success);

  // Take entries from the database and put them in the local cache.
  void LoadEntriesInCache(scoped_ptr<ImageDataVector> entries);

  void ServePendingCacheRequests();

  // From SkBitmap to the vector of JPEG-encoded bytes, |dst|. Visible only for
  // testing.
  static bool EncodeImage(const SkBitmap& bitmap,
                          std::vector<unsigned char>* dest);

  // Map from URL to image URL. Should be kept up to date when a new
  // SuggestionsProfile is available.
  std::map<GURL, GURL> image_url_map_;

  // Map from each image URL to the request information (associated website
  // url, fetcher, pending callbacks).
  ImageRequestMap pending_net_requests_;

  // Map from website URL to request information, used for pending cache
  // requests while the database hasn't loaded.
  ImageRequestMap pending_cache_requests_;

  // Holding the bitmaps in memory, keyed by website URL string.
  ImageMap image_map_;

  net::URLRequestContextGetter* url_request_context_;

  scoped_ptr<leveldb_proto::ProtoDatabase<ImageData> > database_;

  bool database_ready_;

  base::WeakPtrFactory<ImageManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageManagerImpl);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_MANAGER_IMPL_H_
