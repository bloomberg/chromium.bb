// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_CACHE_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_CACHE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/image_fetcher/core/storage/image_store_types.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
class SequencedTaskRunner;
}  // namespace base

namespace image_fetcher {

class ImageDataStore;
class ImageMetadataStore;

// Persist image meta/data via the given implementations of ImageDataStore and
// ImageMetadataStore.
class ImageCache {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  ImageCache(std::unique_ptr<ImageDataStore> data_storage,
             std::unique_ptr<ImageMetadataStore> metadata_storage,
             PrefService* pref_service,
             base::Clock* clock,
             scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ImageCache();

  // Adds or updates the image data for the |url|. If the class hasn't been
  // initialized yet, the call is queued.
  void SaveImage(std::string url, std::string image_data);

  // Loads the image data for the |url| and passes it to |callback|. If there's
  // no image in the cache, then an empty string is returned.
  void LoadImage(std::string url, ImageDataCallback callback);

  // Deletes the image data for the |url|.
  void DeleteImage(std::string url);

 private:
  friend class ImageCacheTest;

  // Queue or start |request| depending if the cache is initialized.
  void QueueOrStartRequest(base::OnceClosure request);
  // Start initializing the stores if it hasn't been started already.
  void MaybeStartInitialization();
  // Returns true iff both the stores have been initialized.
  bool AreAllDependenciesInitialized() const;
  // Receives callbacks when stores are initialized.
  void OnDependencyInitialized();

  // Saves the |image_data| for |url|.
  void SaveImageImpl(const std::string& url, std::string image_data);
  // Loads the data for |url|, calls the user back before updating metadata.
  void LoadImageImpl(const std::string& url, ImageDataCallback callback);
  // Deletes the data for |url|.
  void DeleteImageImpl(const std::string& url);

  // Runs eviction on the data stores.
  void RunEviction();
  // Deletes the given keys from the data store.
  void OnKeysEvicted(std::vector<std::string> keys);

  bool initialization_attempted_;
  std::vector<base::OnceClosure> queued_requests_;

  std::unique_ptr<ImageDataStore> data_store_;
  std::unique_ptr<ImageMetadataStore> metadata_store_;
  PrefService* pref_service_;

  // Owned by the service which instantiates this.
  base::Clock* clock_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ImageCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageCache);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_STORAGE_IMAGE_CACHE_H_
