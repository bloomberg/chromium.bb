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
    GURL url;
    int width;
    int height;
    SkBitmap data;
  };

  WebApplicationInfo();
  ~WebApplicationInfo();

  // URL to a manifest that defines the application. If specified, all other
  // attributes are derived from this manifest, and the manifest is the unique
  // ID of the application.
  GURL manifest_url;

  // Setting indicating this application is artificially constructed. If set,
  // the application was created from bookmark-style data (title, url, possibly
  // icon), and not from an official manifest file. In that case, the app_url
  // can be checked for the later creation of an official manifest instead of
  // reloading the manifest_url.
  bool is_bookmark_app;

  // Title of the application.
  base::string16 title;

  // Description of the application.
  base::string16 description;

  // The launch URL for the app.
  GURL app_url;

  // Set of available icons.
  std::vector<IconInfo> icons;

  // The permissions the app requests. Only supported with manifest-based apps.
  std::vector<std::string> permissions;

  // Set of URLs that comprise the app. Only supported with manifest-based apps.
  // All these must be of the same origin as manifest_url.
  std::vector<GURL> urls;

  // The type of launch container to use with the app. Currently supported
  // values are 'tab' and 'panel'. Only supported with manifest-based apps.
  std::string launch_container;

  // This indicates if the app is functional in offline mode or not.
  bool is_offline_enabled;
};

#endif  // CHROME_COMMON_WEB_APPLICATION_INFO_H_
