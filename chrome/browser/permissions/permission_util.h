// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_

#include <string>

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

class GURL;
class Profile;

namespace content {
enum class PermissionType;
}  // namespace content

struct PermissionTypeHash {
  std::size_t operator()(const content::PermissionType& type) const;
};

// A utility class for permissions.
class PermissionUtil {
 public:
  // Returns the permission string for the given PermissionType.
  static std::string GetPermissionString(content::PermissionType permission);

  // Limited conversion of ContentSettingsType to PermissionType. Intended for
  // recording Permission UMA metrics from areas of the codebase which have not
  // yet been converted to PermissionType. Returns true if the conversion was
  // performed.
  // TODO(tsergeant): Remove this function once callsites operate on
  // PermissionType directly.
  static bool GetPermissionType(ContentSettingsType type,
                                content::PermissionType* out);

  // Helper method which proxies
  // HostContentSettingsMap::SetContentSettingDefaultScope(). Checks the content
  // setting value before and after the change to determine whether it has gone
  // from ALLOW to BLOCK or ASK, and records metrics accordingly. Should be
  // called from UI code when a user changes permissions for a particular origin
  // pair.
  // TODO(tsergeant): This is a temporary solution to begin gathering metrics.
  // We should integrate this better with the permissions layer. See
  // crbug.com/469221.
  static void SetContentSettingAndRecordRevocation(
      Profile* profile,
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      std::string resource_identifier,
      ContentSetting setting);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionUtil);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_
