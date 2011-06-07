// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/about_handler.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

namespace chrome_about_handler {

// This needs to match up with about_urls_handlers in
// chrome/renderer/about_handler.cc.
const char* const about_urls[] = {
  chrome::kAboutCrashURL,
  chrome::kAboutKillURL,
  chrome::kAboutHangURL,
  chrome::kAboutShorthangURL,
  NULL,
};
const size_t about_urls_size = arraysize(about_urls);

const char* const kAboutScheme = "about";

bool WillHandle(const GURL& url) {
  if (url.scheme() != kAboutScheme)
    return false;

  const char* const* url_handler = about_urls;
  while (*url_handler) {
    if (GURL(*url_handler) == url)
      return true;
    url_handler++;
  }
  return false;
}

}  // namespace chrome_about_handler
