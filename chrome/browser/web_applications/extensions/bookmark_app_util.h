// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UTIL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UTIL_H_

namespace content {
class BrowserContext;
}

class Extension;
class GURL;

namespace extensions {

// Sets an extension pref to indicate whether the hosted app is locally
// installed or not. When apps are not locally installed they will appear in the
// app launcher, but will act like normal web pages when launched. For example
// they will never open in standalone windows. They will also have different
// commands available to them reflecting the fact that they aren't fully
// installed.
void SetBookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                      const Extension* extension,
                                      bool is_locally_installed);

// Gets whether the bookmark app is locally installed. Defaults to true if the
// extension pref that stores this isn't set.
// Note this can be called for hosted apps which should use the default.
bool BookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                   const Extension* extension);

// Returns true if a bookmark or hosted app from a given URL is already
// installed and enabled.
bool BookmarkOrHostedAppInstalled(content::BrowserContext* browser_context,
                                  const GURL& url);

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UTIL_H_
