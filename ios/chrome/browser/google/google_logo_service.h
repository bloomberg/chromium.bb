// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_GOOGLE_LOGO_SERVICE_H_
#define IOS_CHROME_BROWSER_GOOGLE_GOOGLE_LOGO_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/search_provider_logos/logo_tracker.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ios {
class ChromeBrowserState;
}

// Provides the logo if a BrowserState's default search provider is Google.
//
// Example usage:
//   GoogleLogoService* logo_service =
//       GoogleLogoServiceFactory::GetForBrowserState(browser_state);
//   logo_service->GetLogo(...);
//
class GoogleLogoService : public KeyedService {
 public:
  explicit GoogleLogoService(ios::ChromeBrowserState* browser_state);
  ~GoogleLogoService() override;

  // Gets the logo for the default search provider and notifies |observer|
  // with the results.
  void GetLogo(search_provider_logos::LogoObserver* observer);

  // |LogoTracker| does everything on callbacks, and iOS needs to load the logo
  // immediately on page load. This caches the SkBitmap so we can immediately
  // load. This prevents showing the google logo on every new tab page and
  // immediately animating to the logo. Only one SkBitmap is cached per
  // BrowserState.
  void SetCachedLogo(const search_provider_logos::Logo* logo);
  search_provider_logos::Logo GetCachedLogo();

 private:
  ios::ChromeBrowserState* browser_state_;
  std::unique_ptr<search_provider_logos::LogoTracker> logo_tracker_;
  SkBitmap cached_image_;
  search_provider_logos::LogoMetadata cached_metadata_;
  const search_provider_logos::LogoMetadata empty_metadata;

  DISALLOW_COPY_AND_ASSIGN(GoogleLogoService);
};

#endif  // IOS_CHROME_BROWSER_GOOGLE_GOOGLE_LOGO_SERVICE_H_
