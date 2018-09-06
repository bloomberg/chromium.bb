// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a class that contains various method related to branding.
// It provides only default implementations of these methods. Usually to add
// specific branding, we will need to extend this class with a custom
// implementation.

#include "chrome/installer/util/browser_distribution.h"

#include <utility>

#include "base/atomicops.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/non_updating_app_registration_data.h"

namespace {

// The BrowserDistribution object is never freed.
BrowserDistribution* g_browser_distribution = NULL;

}  // namespace

BrowserDistribution::BrowserDistribution()
    : app_reg_data_(std::make_unique<NonUpdatingAppRegistrationData>(
          L"Software\\Chromium")) {}

BrowserDistribution::BrowserDistribution(
    std::unique_ptr<AppRegistrationData> app_reg_data)
    : app_reg_data_(std::move(app_reg_data)) {}

BrowserDistribution::~BrowserDistribution() = default;

template<class DistributionClass>
BrowserDistribution* BrowserDistribution::GetOrCreateBrowserDistribution(
    BrowserDistribution** dist) {
  if (!*dist) {
    DistributionClass* temp = new DistributionClass();
    if (base::subtle::NoBarrier_CompareAndSwap(
            reinterpret_cast<base::subtle::AtomicWord*>(dist), NULL,
            reinterpret_cast<base::subtle::AtomicWord>(temp)) != NULL)
      delete temp;
  }

  return *dist;
}

// static
BrowserDistribution* BrowserDistribution::GetDistribution() {
  BrowserDistribution* dist = NULL;

#if defined(GOOGLE_CHROME_BUILD)
  dist = GetOrCreateBrowserDistribution<GoogleChromeDistribution>(
      &g_browser_distribution);
#else
  dist = GetOrCreateBrowserDistribution<BrowserDistribution>(
      &g_browser_distribution);
#endif

  return dist;
}

const AppRegistrationData& BrowserDistribution::GetAppRegistrationData() const {
  return *app_reg_data_;
}

base::string16 BrowserDistribution::GetStateKey() const {
  return app_reg_data_->GetStateKey();
}

base::string16 BrowserDistribution::GetStateMediumKey() const {
  return app_reg_data_->GetStateMediumKey();
}

base::string16 BrowserDistribution::GetVersionKey() const {
  return app_reg_data_->GetVersionKey();
}
