// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_CONFIG_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_CONFIG_H_
#pragma once

#include "base/time.h"
#include "ui/gfx/rect.h"

namespace prerender {

struct Config {
  Config();

  // Maximum memory use for a prerendered page until it is killed.
  size_t max_bytes;

  // Number of simultaneous prendered pages allowed.
  unsigned int max_elements;

  // Is rate limiting enabled?
  bool rate_limit_enabled;

  // Maximum age for a prerendered page until it is removed.
  base::TimeDelta max_age;

  // Is https allowed?
  bool https_allowed;

  // The default tab bounds used as the prerenderer tab size when the active tab
  // cannot be accessed.
  gfx::Rect default_tab_bounds;

  // User agent being used as an override for the WebContents.
  std::string user_agent_override;

  // Is the user agent being overridden for this NavigationEntry?
  bool is_overriding_user_agent;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_CONFIG_H_
