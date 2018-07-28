// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_helpers.h"

#include "base/strings/strcat.h"
#include "url/gurl.h"

namespace web_app {

std::string GenerateApplicationNameFromURL(const GURL& url) {
  return base::StrCat({url.host_piece(), "_", url.path_piece()});
}

// The following string is used to build the directory name for
// shortcuts to chrome applications (the kind which are installed
// from a CRX).  Application shortcuts to URLs use the {host}_{path}
// for the name of this directory.  Hosts can't include an underscore.
// By starting this string with an underscore, we ensure that there
// are no naming conflicts.
static const char kCrxAppPrefix[] = "_crx_";

std::string GenerateApplicationNameFromAppId(const std::string& app_id) {
  std::string t(kCrxAppPrefix);
  t.append(app_id);
  return t;
}

std::string GetAppIdFromApplicationName(const std::string& app_name) {
  std::string prefix(kCrxAppPrefix);
  if (app_name.substr(0, prefix.length()) != prefix)
    return std::string();
  return app_name.substr(prefix.length());
}

}  // namespace web_app
