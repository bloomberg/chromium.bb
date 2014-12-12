// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_IMAGE_MANAGER_H_
#define COMPONENTS_SUGGESTIONS_IMAGE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/suggestions/image_fetcher_delegate.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace suggestions {

class ImageData;
class ImageFetcher;
class SuggestionsProfile;

// A class used to fetch server images asynchronously and manage the caching
// layer (both in memory and on disk).
class ImageManager : public ImageFetcherDelegate {
 public:
  typedef std::vector<ImageData> ImageDataVector;

  ImageManager(scoped_ptr<ImageFetcher> image_fetcher,
               scoped_ptr<leveldb_proto::ProtoDatabase<ImageData> > database,
               const base::FilePath& database_dir);
  ~ImageManager() override;

  virtual void Initialize(const SuggestionsProfile& suggestions);

  // Should be called from the UI thread.
  virtual void GetImageForURL(
      const GURL& url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback);

 protected:
  // Perform additional tasks when an image has been fetched.
  void OnImageFetched(const GURL& url, const SkBitmap* bitmap) override;

 private:
  friend class MockImageManager;
  friend class ImageManagerTest;
  FRIEND_TEST_ALL_PREFIXES(ImageManagerTest, InitializeTest);
  FRIEND_TEST_ALL_PREFIXES(ImageManagerTest, GetImageForURLNetworkCacheHit);
  FRIEND_TEST_ALL_PREFIXES(ImageManagerTest,
                           GetImageForURLNetworkCacheNotInitialized);

  // Used for testing.
  ImageManager();

  typedef std::vector<base::Callback<void(const GURL&, const SkBitmap*)> >
      CallbackVector;
  typedef base::hash_map<std::string, SkBitmap> ImageMap;

  // State related to an image fetch (associated website url, image_url,
  // pending callbacks).
  struct ImageCacheRequest {
    ImageCacheRequest();
    ~ImageCacheRequest();

    GURL url;
    GURL image_url;
    // Queue for pending callbacks, which may accumulate while the request is in
    // flight.
    CallbackVector callbacks;
  };

  typedef std::map<const GURL, ImageCacheRequest> ImageCacheRequestMap;

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

  // Map from URL to image URL. Should be kept up to date when a new
  // SuggestionsProfile is available.
  std::map<GURL, GURL> image_url_map_;

  // Map from website URL to request information, used for pending cache
  // requests while the database hasn't loaded.
  ImageCacheRequestMap pending_cache_requests_;

  // Holding the bitmaps in memory, keyed by website URL string.
  ImageMap image_map_;

  scoped_ptr<ImageFetcher> image_fetcher_;

  scoped_ptr<leveldb_proto::ProtoDatabase<ImageData> > database_;

  bool database_ready_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<ImageManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageManager);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_IMAGE_MANAGER_H_
