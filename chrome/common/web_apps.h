// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WEB_APPS_H_
#define CHROME_COMMON_WEB_APPS_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

namespace WebKit {
class WebDocument;
class WebFrame;
}

class Value;

// Structure used when installing a web page as an app.
struct WebApplicationInfo {
  struct IconInfo {
    GURL url;
    int width;
    int height;
    SkBitmap data;
  };

  static const char kInvalidDefinitionURL[];
  static const char kInvalidLaunchURL[];
  static const char kInvalidURL[];
  static const char kInvalidIconSize[];
  static const char kInvalidIconURL[];

  WebApplicationInfo();
  ~WebApplicationInfo();

  // URL to a manifest that defines the application. If specified, all other
  // attributes are derived from this manifest, and the manifest is the unique
  // ID of the application.
  GURL manifest_url;

  // Title of the application.
  string16 title;

  // Description of the application.
  string16 description;

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
};


namespace web_apps {

// Parses an icon size. An icon size must match the following regex:
// [1-9][0-9]*x[1-9][0-9]*.
// If the input couldn't be parsed, a size with a width/height == 0 is returned.
gfx::Size ParseIconSize(const string16& text);

// Parses the icon's size attribute as defined in the HTML 5 spec. Returns true
// on success, false on errors. On success either all the sizes specified in
// the attribute are added to sizes, or is_any is set to true.
//
// You shouldn't have a need to invoke this directly, it's public for testing.
bool ParseIconSizes(const string16& text, std::vector<gfx::Size>* sizes,
                    bool* is_any);

// Parses |web_app| information out of the document in frame. Returns true on
// success, or false and |error| on failure. Note that the document may contain
// no web application information, in which case |web_app| is unchanged and the
// function returns true.
//
// Documents can also contain a link to a application 'definition'. In this case
// web_app will have manifest_url set and nothing else. The caller must fetch
// this URL and pass the result to ParseWebAppFromDefinitionFile() for further
// processing.
bool ParseWebAppFromWebDocument(WebKit::WebFrame* frame,
                                WebApplicationInfo* web_app,
                                string16* error);

// Parses |web_app| information out of |definition|. Returns true on success, or
// false and |error| on failure. This function assumes that |web_app| has a
// valid manifest_url.
bool ParseWebAppFromDefinitionFile(Value* definition,
                                   WebApplicationInfo* web_app,
                                   string16* error);

}  // namespace web_apps

#endif  // CHROME_COMMON_WEB_APPS_H_
