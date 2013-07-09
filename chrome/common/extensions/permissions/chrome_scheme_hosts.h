// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_SCHEME_HOSTS_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_SCHEME_HOSTS_H_

#include "chrome/common/extensions/permissions/permission_message.h"

// Chrome-specific special case handling for permissions on hosts in
// the chrome:// scheme.
namespace extensions {

class APIPermissionSet;
class Extension;
class URLPatternSet;

PermissionMessages GetChromeSchemePermissionWarnings(
    const URLPatternSet& hosts);

URLPatternSet GetPermittedChromeSchemeHosts(
    const Extension* extension,
    const APIPermissionSet& permissions);

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_SCHEME_HOSTS_H_
