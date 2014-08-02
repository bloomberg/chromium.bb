// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_helper.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/child_process_security_policy.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"
#include "webkit/browser/quota/special_storage_policy.h"

// Static
bool BrowsingDataHelper::IsWebScheme(const std::string& scheme) {
  // Special-case `file://` scheme iff cookies and site data are enabled via
  // the `--allow-file-cookies` CLI flag.
  if (scheme == url::kFileScheme) {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableFileCookies);
  }

  // Otherwise, all "web safe" schemes are valid, except `chrome-extension://`
  // and `chrome-devtools://`.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  return (policy->IsWebSafeScheme(scheme) &&
          !BrowsingDataHelper::IsExtensionScheme(scheme) &&
          scheme != content::kChromeDevToolsScheme);
}

// Static
bool BrowsingDataHelper::HasWebScheme(const GURL& origin) {
  return BrowsingDataHelper::IsWebScheme(origin.scheme());
}

// Static
bool BrowsingDataHelper::IsExtensionScheme(const std::string& scheme) {
  return scheme == extensions::kExtensionScheme;
}

// Static
bool BrowsingDataHelper::HasExtensionScheme(const GURL& origin) {
  return BrowsingDataHelper::IsExtensionScheme(origin.scheme());
}

// Static
bool BrowsingDataHelper::DoesOriginMatchMask(const GURL& origin,
    int origin_set_mask, quota::SpecialStoragePolicy* policy) {
  // Packaged apps and extensions match iff EXTENSION.
  if (BrowsingDataHelper::HasExtensionScheme(origin.GetOrigin()) &&
      origin_set_mask & EXTENSION)
    return true;

  // If a websafe origin is unprotected, it matches iff UNPROTECTED_WEB.
  if ((!policy || !policy->IsStorageProtected(origin.GetOrigin())) &&
      BrowsingDataHelper::HasWebScheme(origin.GetOrigin()) &&
      origin_set_mask & UNPROTECTED_WEB)
    return true;

  // Hosted applications (protected and websafe origins) iff PROTECTED_WEB.
  if (policy &&
      policy->IsStorageProtected(origin.GetOrigin()) &&
      BrowsingDataHelper::HasWebScheme(origin.GetOrigin()) &&
      origin_set_mask & PROTECTED_WEB)
    return true;

  return false;
}
