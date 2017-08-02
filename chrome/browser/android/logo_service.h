// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOGO_SERVICE_H_
#define CHROME_BROWSER_ANDROID_LOGO_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace search_provider_logos {
class LogoTracker;
class LogoObserver;
}  // namespace search_provider_logos

// Provides the logo for a profile's default search provider.
//
// Example usage:
//   LogoService* logo_service = LogoServiceFactory::GetForProfile(profile);
//   logo_service->GetLogo(...);
//
class LogoService : public KeyedService {
 public:
  LogoService(Profile* profile, bool use_gray_background);
  ~LogoService() override;

  // Gets the logo for the default search provider and notifies |observer|
  // with the results.
  void GetLogo(search_provider_logos::LogoObserver* observer);

 private:
  Profile* profile_;
  const bool use_gray_background_;
  std::unique_ptr<search_provider_logos::LogoTracker> logo_tracker_;

  DISALLOW_COPY_AND_ASSIGN(LogoService);
};

#endif  // CHROME_BROWSER_ANDROID_LOGO_SERVICE_H_
