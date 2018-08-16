// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_HELPERS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_HELPERS_H_

#include <string>

class GURL;

namespace web_app {

// Compute a deterministic name based on the URL. We use this pseudo name
// as a key to store window location per application URLs in Browser and
// as app id for BrowserWindow, shortcut and jump list.
std::string GenerateApplicationNameFromURL(const GURL& url);

// Compute a deterministic name based on an apps's id.
std::string GenerateApplicationNameFromAppId(const std::string& app_id);

// Extracts the application id from the app name.
std::string GetAppIdFromApplicationName(const std::string& app_name);

// Compute the Extension ID (such as "fedbieoalmbobgfjapopkghdmhgncnaa") or
// Extension Key, from a web app's URL. Both are derived from a hash of the
// URL, but are subsequently encoded differently, for historical reasons. The
// ID is a Base-16 encoded (a=0, b=1, ..., p=15) subset of the hash, and is
// used as a directory name, sometimes on case-insensitive file systems
// (Windows). The Key is a Base-64 encoding of the hash.
//
// For PWAs (progressive web apps), the URL should be the Start URL, explicitly
// listed in the manifest.
//
// For non-PWA web apps, also known as "bookmark apps", the URL is just the
// bookmark URL.
std::string GenerateExtensionIdFromURL(const GURL& url);
std::string GenerateExtensionKeyFromURL(const GURL& url);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_HELPERS_H_
