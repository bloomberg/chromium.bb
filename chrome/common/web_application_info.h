// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WEB_APPLICATION_INFO_H_
#define CHROME_COMMON_WEB_APPLICATION_INFO_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

// Structure used when installing a web page as an app.
struct WebApplicationInfo {
  struct IconInfo {
    IconInfo();
    ~IconInfo();

    GURL url;
    int width;
    int height;
    SkBitmap data;
  };

  WebApplicationInfo();
  ~WebApplicationInfo();

  // Title of the application.
  base::string16 title;

  // Description of the application.
  base::string16 description;

  // The launch URL for the app.
  GURL app_url;

  // Set of available icons.
  std::vector<IconInfo> icons;
};

#endif  // CHROME_COMMON_WEB_APPLICATION_INFO_H_
