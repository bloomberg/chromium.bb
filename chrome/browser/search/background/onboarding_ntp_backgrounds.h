// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_ONBOARDING_NTP_BACKGROUNDS_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_ONBOARDING_NTP_BACKGROUNDS_H_

#include <array>

#include "url/gurl.h"

const size_t kOnboardingNtpBackgroundsCount = 5;
std::array<GURL, kOnboardingNtpBackgroundsCount> GetOnboardingNtpBackgrounds();

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_ONBOARDING_NTP_BACKGROUNDS_H_
