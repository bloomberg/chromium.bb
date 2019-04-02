// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/cache/image_data_store_disk.h"
#include "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace image_fetcher {

namespace {

// The path under the browser context's data directory which the image_cache
// will be stored.
const base::FilePath::CharType kImageCacheSubdir[] =
    FILE_PATH_LITERAL("image_cache");

}  // namespace

// static
base::FilePath ImageFetcherServiceFactory::GetCachePath(SimpleFactoryKey* key) {
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(key->path(), &cache_path);
  return cache_path.Append(kImageCacheSubdir);
}

// static
ImageFetcherService* ImageFetcherServiceFactory::GetForKey(
    SimpleFactoryKey* key,
    PrefService* prefs) {
  return static_cast<ImageFetcherService*>(
      GetInstance()->GetServiceForKey(key, prefs, true));
}

// static
ImageFetcherServiceFactory* ImageFetcherServiceFactory::GetInstance() {
  return base::Singleton<ImageFetcherServiceFactory>::get();
}

ImageFetcherServiceFactory::ImageFetcherServiceFactory()
    : SimpleKeyedServiceFactory("ImageFetcherService",
                                SimpleDependencyManager::GetInstance()) {}

ImageFetcherServiceFactory::~ImageFetcherServiceFactory() = default;

std::unique_ptr<KeyedService>
ImageFetcherServiceFactory::BuildServiceInstanceFor(SimpleFactoryKey* key,
                                                    PrefService* prefs) const {
  base::FilePath cache_path = GetCachePath(key);

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  base::DefaultClock* clock = base::DefaultClock::GetInstance();

  auto metadata_store = std::make_unique<ImageMetadataStoreLevelDB>(
      cache_path, task_runner, clock);
  auto data_store =
      std::make_unique<ImageDataStoreDisk>(cache_path, task_runner);

  scoped_refptr<ImageCache> image_cache = base::MakeRefCounted<ImageCache>(
      std::move(data_store), std::move(metadata_store), prefs, clock,
      task_runner);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory();

  auto cached_image_fetcher_service = std::make_unique<ImageFetcherService>(
      std::make_unique<suggestions::ImageDecoderImpl>(),
      std::move(url_loader_factory), std::move(image_cache),
      key->is_off_the_record());
  return cached_image_fetcher_service;
}

SimpleFactoryKey* ImageFetcherServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}

}  // namespace image_fetcher
