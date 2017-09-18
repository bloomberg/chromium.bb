// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_ANDROID_LOGO_SERVICE_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_ANDROID_LOGO_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_provider_logos/logo_common.h"

class TemplateURLService;

namespace base {
class Clock;
}  // namespace base

namespace image_fetcher {
class ImageDecoder;
}  // namespace image_fetcher

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace search_provider_logos {

class LogoCache;
class LogoTracker;
class LogoObserver;

// Provides the logo for a profile's default search provider.
//
// Example usage:
//   LogoService* logo_service = LogoServiceFactory::GetForProfile(profile);
//   logo_service->GetLogo(...);
//
class LogoService : public KeyedService {
 public:
  LogoService(
      const base::FilePath& cache_directory,
      TemplateURLService* template_url_service,
      std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      bool use_gray_background);

  ~LogoService() override;

  // Gets the logo for the default search provider and calls the provided
  // callbacks with the encoded and decoded logos. The service will:
  //
  // 1.  Load a cached logo, and call callbacks.on_cached_{en,de}coded_logo.
  // 2.  Fetch a fresh logo, and call callbacks.on_fresh_{en,de}coded_logo.
  //
  // At least one member of |callbacks| must be non-null.
  void GetLogo(LogoCallbacks callbacks);

  // Gets the logo for the default search provider and notifies |observer|
  // 0-2 times with the results. The service will:
  //
  // 1.  Call observer->OnLogoAvailable() with |from_cache=true| when
  //     |on_cached_decoded_logo_available| would be called in the callback
  //     interface with type DETERMINED.
  // 2.  Call observer->OnLogoAvailable() with |from_cache=false| when
  //     |on_fresh_decoded_logo_available| would be called in the callback
  //     interface with type DETERMINED.
  // 3.  Call observer->OnObserverRemoved().
  void GetLogo(LogoObserver* observer);

  // Overrides the cache used to store logos.
  void SetLogoCacheForTests(std::unique_ptr<LogoCache> cache);

  // Overrides the clock used to check the time.
  void SetClockForTests(std::unique_ptr<base::Clock> clock);

 private:
  // Constructor arguments.
  const base::FilePath cache_directory_;
  TemplateURLService* const template_url_service_;
  const scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  const bool use_gray_background_;

  // logo_tracker_ takes ownership if/when it is initialized.
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;

  // For testing. logo_tracker_ takes ownership if/when it is initialized.
  std::unique_ptr<base::Clock> clock_for_test_;
  std::unique_ptr<LogoCache> logo_cache_for_test_;

  // Lazily initialized on first call to GetLogo().
  std::unique_ptr<search_provider_logos::LogoTracker> logo_tracker_;

  DISALLOW_COPY_AND_ASSIGN(LogoService);
};

}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_ANDROID_LOGO_SERVICE_H_
