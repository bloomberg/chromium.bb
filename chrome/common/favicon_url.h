// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_FAVICON_URL_
#define CHROME_COMMON_FAVICON_URL_
#pragma once

#include "googleurl/src/gurl.h"

// The favicon url from the render.
struct FaviconURL {
  // The icon type in a page. The definition must be same as history::IconType.
  enum IconType {
    INVALID_ICON = 0x0,
    FAVICON = 1 << 0,
    TOUCH_ICON = 1 << 1,
    TOUCH_PRECOMPOSED_ICON = 1 << 2
  };

  FaviconURL();
  FaviconURL(const GURL& url, IconType type);
  ~FaviconURL();

  // The url of the icon.
  GURL icon_url;

  // The type of the icon
  IconType icon_type;
};

#endif  // CHROME_COMMON_FAVICON_URL_
