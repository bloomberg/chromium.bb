// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cached_image_fetcher/cached_image_fetcher_service_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/common/chrome_paths_internal.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/cache/image_data_store_disk.h"
#include "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"
#include "components/image_fetcher/core/cached_image_fetcher_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
base::FilePath CachedImageFetcherServiceFactory::GetCachePath(
    Profile* profile) {
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(profile->GetPath(), &cache_path);
  return cache_path.Append(kImageCacheSubdir);
}

// static
CachedImageFetcherService*
CachedImageFetcherServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CachedImageFetcherService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
CachedImageFetcherServiceFactory*
CachedImageFetcherServiceFactory::GetInstance() {
  return base::Singleton<CachedImageFetcherServiceFactory>::get();
}

CachedImageFetcherServiceFactory::CachedImageFetcherServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "CachedImageFetcherService",
          BrowserContextDependencyManager::GetInstance()) {}

CachedImageFetcherServiceFactory::~CachedImageFetcherServiceFactory() = default;

KeyedService* CachedImageFetcherServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  base::FilePath cache_path = GetCachePath(profile);

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  base::DefaultClock* clock = base::DefaultClock::GetInstance();

  auto metadata_store = std::make_unique<ImageMetadataStoreLevelDB>(
      cache_path, task_runner, clock);
  auto data_store =
      std::make_unique<ImageDataStoreDisk>(cache_path, task_runner);

  scoped_refptr<ImageCache> image_cache = base::MakeRefCounted<ImageCache>(
      std::move(data_store), std::move(metadata_store), profile->GetPrefs(),
      clock, task_runner);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();

  return new CachedImageFetcherService(
      std::make_unique<suggestions::ImageDecoderImpl>(),
      std::move(url_loader_factory), std::move(image_cache),
      context->IsOffTheRecord());
}

content::BrowserContext*
CachedImageFetcherServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Return different BrowserContexts for regular/incognito.
  return context;
}

}  // namespace image_fetcher
