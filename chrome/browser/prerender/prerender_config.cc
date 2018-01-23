// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_config.h"

namespace {

// The test_timeouts.cc defines general timeout multipliers for Asan builds.
// Increase the timeout similarly here.
#if defined(ADDRESS_SANITIZER)
constexpr int kTimeToLiveMinutes = 10;
#else
constexpr int kTimeToLiveMinutes = 5;
#endif

}  // namespace

namespace prerender {

Config::Config()
    : max_bytes(150 * 1024 * 1024),
      max_link_concurrency(1),
      max_link_concurrency_per_launcher(1),
      rate_limit_enabled(true),
      max_wait_to_launch(base::TimeDelta::FromMinutes(4)),
      time_to_live(base::TimeDelta::FromMinutes(kTimeToLiveMinutes)),
      abandon_time_to_live(base::TimeDelta::FromSeconds(3)),
      default_tab_bounds(640, 480),
      is_overriding_user_agent(false) {}

Config::~Config() { }

}  // namespace prerender
