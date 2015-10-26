// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_

#include <string>

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings_types.h"

// A utility class for permissions.
class PermissionUtil {
 public:
  // Returns the permission string for the given ContentSettingsType.
  static std::string GetPermissionString(ContentSettingsType permission);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionUtil);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_
