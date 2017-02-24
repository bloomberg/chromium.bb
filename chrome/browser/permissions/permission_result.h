// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_RESULT_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_RESULT_H_

#include "components/content_settings/core/common/content_settings.h"

// Identifies the source or reason for a permission status being returned. This
// enum backs an UMA histogram and must be treated as append-only.
enum class PermissionStatusSource {
  // The reason for the status is not specified.
  UNSPECIFIED,

  // The status is the result of being blocked due to the user dismissing a
  // permission prompt multiple times.
  MULTIPLE_DISMISSALS,

  // The status is the result of being blocked because the permission is on the
  // safe browsing blacklist.
  SAFE_BROWSING_BLACKLIST,

  // The status is the result of being blocked by the permissions kill switch.
  KILL_SWITCH,
};

struct PermissionResult {
  PermissionResult(ContentSetting content_setting,
                   PermissionStatusSource source);
  ~PermissionResult();

  ContentSetting content_setting;
  PermissionStatusSource source;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_RESULT_H_
