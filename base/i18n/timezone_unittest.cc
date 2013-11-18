// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/timezone.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

#if defined(OS_ANDROID)
// icu's timezone utility doesn't work on Android.
#define MAYBE_CountryCodeForCurrentTimezone DISABLED_CountryCodeForCurrentTimezone
#else
#define MAYBE_CountryCodeForCurrentTimezone CountryCodeForCurrentTimezone
#endif

TEST(TimezoneTest, MAYBE_CountryCodeForCurrentTimezone) {
  std::string country_code = CountryCodeForCurrentTimezone();
  EXPECT_EQ(2U, country_code.size());
}

}  // namespace
}  // namespace base
