// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/storage/image_cache.h"

#include <utility>

#include "base/bind.h"
#include "base/sha1.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/base32/base32.h"
#include "components/image_fetcher/core/storage/image_data_store.h"
#include "components/image_fetcher/core/storage/image_metadata_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kPrefLastEvictionKey[] =
    "cached_image_fetcher_last_eviction_time";

// TODO(wylieb): Control these parameters server-side.
constexpr size_t kCacheMaxSize = 64 * 1024 * 1024;  // 64mb.

// Cache items are allowed to live for the given amount of days.
constexpr size_t kCacheItemsTimeToLiveDays = 7;
constexpr size_t kImageCacheEvictionIntervalHours = 24;
constexpr size_t kImageCacheEvictionDelayMinutes = 5;

std::string HashUrlToKey(const std::string& input) {
  return base32::Base32Encode(base::SHA1HashString(input));
}

}  // namespace

namespace image_fetcher {

// static
void ImageCache::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kPrefLastEvictionKey, base::Time());
}

ImageCache::ImageCache(std::unique_ptr<ImageDataStore> data_store,
                       std::unique_ptr<ImageMetadataStore> metadata_store,
                       PrefService* pref_service,
                       base::Clock* clock,
                       scoped_refptr<base::SequencedTaskRunner> task_runner)
    : initialization_attempted_(false),
      data_store_(std::move(data_store)),
      metadata_store_(std::move(metadata_store)),
      pref_service_(pref_service),
      clock_(clock),
      task_runner_(task_runner),
      weak_ptr_factory_(this) {}

ImageCache::~ImageCache() = default;

void ImageCache::SaveImage(std::string url, std::string image_data) {
  // If the image data is larger than the cache's max size, bail out.
  if (image_data.length() > kCacheMaxSize) {
    return;
  }

  base::OnceClosure request =
      base::BindOnce(&ImageCache::SaveImageImpl, weak_ptr_factory_.GetWeakPtr(),
                     url, std::move(image_data));
  QueueOrStartRequest(std::move(request));
}

void ImageCache::LoadImage(std::string url, ImageDataCallback callback) {
  base::OnceClosure request =
      base::BindOnce(&ImageCache::LoadImageImpl, weak_ptr_factory_.GetWeakPtr(),
                     url, std::move(callback));
  QueueOrStartRequest(std::move(request));
}

void ImageCache::DeleteImage(std::string url) {
  base::OnceClosure request = base::BindOnce(
      &ImageCache::DeleteImageImpl, weak_ptr_factory_.GetWeakPtr(), url);
  QueueOrStartRequest(std::move(request));
}

void ImageCache::QueueOrStartRequest(base::OnceClosure request) {
  if (!AreAllDependenciesInitialized()) {
    queued_requests_.push_back(std::move(request));
    MaybeStartInitialization();
    return;
  }

  // Post task for fairness with tasks that may be queued.
  task_runner_->PostTask(FROM_HERE, std::move(request));
}

void ImageCache::MaybeStartInitialization() {
  if (initialization_attempted_) {
    return;
  }

  initialization_attempted_ = true;
  data_store_->Initialize(base::BindOnce(&ImageCache::OnDependencyInitialized,
                                         weak_ptr_factory_.GetWeakPtr()));
  metadata_store_->Initialize(base::BindOnce(
      &ImageCache::OnDependencyInitialized, weak_ptr_factory_.GetWeakPtr()));
}

bool ImageCache::AreAllDependenciesInitialized() const {
  return data_store_->IsInitialized() && metadata_store_->IsInitialized();
}

void ImageCache::OnDependencyInitialized() {
  if (!AreAllDependenciesInitialized()) {
    return;
  }

  // Everything is initialized, take care of the queued requests.
  for (base::OnceClosure& request : queued_requests_) {
    task_runner_->PostTask(FROM_HERE, std::move(request));
  }
  queued_requests_.clear();

  // TODO(wylieb): Consider delaying eviction as new requests come in via
  // seperate weak pointers.
  // Once all the queued requests are taken care of, run eviction.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ImageCache::RunEviction, weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMinutes(kImageCacheEvictionDelayMinutes));
}

void ImageCache::SaveImageImpl(const std::string& url, std::string image_data) {
  std::string key = HashUrlToKey(url);

  // TODO(wylieb): Run eviction if size is larger than desired size.
  size_t length = image_data.length();
  data_store_->SaveImage(key, std::move(image_data));
  metadata_store_->SaveImageMetadata(key, length);
}

void ImageCache::LoadImageImpl(const std::string& url,
                               ImageDataCallback callback) {
  std::string key = HashUrlToKey(url);

  data_store_->LoadImage(key, std::move(callback));
  metadata_store_->UpdateImageMetadata(key);
}

void ImageCache::DeleteImageImpl(const std::string& url) {
  std::string key = HashUrlToKey(url);

  data_store_->DeleteImage(key);
  metadata_store_->DeleteImageMetadata(key);
}

// TODO(wylieb): Support an eviction and reconciliation routine.
void ImageCache::RunEviction() {
  base::Time last_eviction_time = pref_service_->GetTime(kPrefLastEvictionKey);
  // If we've already garbage collected in the past interval, bail out.
  if (last_eviction_time >
      clock_->Now() -
          base::TimeDelta::FromHours(kImageCacheEvictionIntervalHours)) {
    return;
  }

  base::Time eviction_time = clock_->Now();
  pref_service_->SetTime(kPrefLastEvictionKey, eviction_time);
  metadata_store_->EvictImageMetadata(
      eviction_time - base::TimeDelta::FromDays(kCacheItemsTimeToLiveDays),
      kCacheMaxSize,
      base::BindOnce(&ImageCache::OnKeysEvicted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageCache::OnKeysEvicted(std::vector<std::string> keys) {
  for (const std::string& key : keys) {
    data_store_->DeleteImage(key);
  }
}

}  // namespace image_fetcher
