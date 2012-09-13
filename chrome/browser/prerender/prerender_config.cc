// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_config.h"

namespace prerender {

Config::Config() : max_bytes(100 * 1024 * 1024),
                   max_concurrency(1),
                   rate_limit_enabled(true),
                   time_to_live(base::TimeDelta::FromMinutes(5)),
                   abandon_time_to_live(base::TimeDelta::FromSeconds(30)),
                   https_allowed(true),
                   default_tab_bounds(640, 480),
                   is_overriding_user_agent(false) {
}

Config::~Config() { }

}  // namespace prerender
