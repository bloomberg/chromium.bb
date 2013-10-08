// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/permissions/chrome_scheme_hosts.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"

namespace {
const char kThumbsWhiteListedExtension[] = "khopmbdjffemhegeeobelklnbglcdgfh";
}  // namespace

namespace extensions {

URLPatternSet GetPermittedChromeSchemeHosts(
    const Extension* extension,
    const APIPermissionSet& api_permissions) {
  URLPatternSet hosts;
  // Regular extensions are only allowed access to chrome://favicon.
  hosts.AddPattern(URLPattern(URLPattern::SCHEME_CHROMEUI,
                              chrome::kChromeUIFaviconURL));

  // Experimental extensions are also allowed chrome://thumb.
  //
  // TODO: A public API should be created for retrieving thumbnails.
  // See http://crbug.com/222856. A temporary hack is implemented here to
  // make chrome://thumbs available to NTP Russia extension as
  // non-experimental.
  if ((api_permissions.find(APIPermission::kExperimental) !=
       api_permissions.end()) ||
      (extension->id() == kThumbsWhiteListedExtension &&
       extension->from_webstore())) {
    hosts.AddPattern(URLPattern(URLPattern::SCHEME_CHROMEUI,
                                chrome::kChromeUIThumbnailURL));
  }
  return hosts;
}

}  // namespace extensions
