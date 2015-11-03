// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_

#include "base/logging.h"
#include "components/content_settings/core/common/content_settings_types.h"

class GURL;
class Profile;

// Provides a convenient way of logging UMA for permission related operations.
class PermissionUmaUtil {
 public:
  static void PermissionRequested(ContentSettingsType permission,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  Profile* profile);
  static void PermissionGranted(ContentSettingsType permission,
                                const GURL& requesting_origin);
  static void PermissionDenied(ContentSettingsType permission,
                               const GURL& requesting_origin);
  static void PermissionDismissed(ContentSettingsType permission,
                                  const GURL& requesting_origin);
  static void PermissionIgnored(ContentSettingsType permission,
                                const GURL& requesting_origin);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionUmaUtil);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_
