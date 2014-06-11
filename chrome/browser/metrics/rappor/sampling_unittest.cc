// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/rappor/sampling.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace rappor {

// Make sure recording a sample during tests, when the Rappor service is NULL,
// doesn't cause a crash.
TEST(RapporSamplingTest, SmokeTest) {
  SampleDomainAndRegistryFromHost(std::string(), std::string());
  SampleDomainAndRegistryFromGURL(std::string(), GURL());
}

}  // namespace rappor
