// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_

#include <vector>

#include "base/logging.h"
#include "base/macros.h"

class GURL;
class Profile;

namespace content {
enum class PermissionType;
}  // namespace content

class PermissionBubbleRequest;

// Enum for UMA purposes, make sure you update histograms.xml if you add new
// permission actions. Never delete or reorder an entry; only add new entries
// immediately before PERMISSION_NUM
enum PermissionAction {
  GRANTED = 0,
  DENIED = 1,
  DISMISSED = 2,
  IGNORED = 3,
  REVOKED = 4,
  REENABLED = 5,
  REQUESTED = 6,

  // Always keep this at the end.
  PERMISSION_ACTION_NUM,
};

// Provides a convenient way of logging UMA for permission related operations.
class PermissionUmaUtil {
 public:
  static void PermissionRequested(content::PermissionType permission,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  Profile* profile);
  static void PermissionGranted(content::PermissionType permission,
                                const GURL& requesting_origin);
  static void PermissionDenied(content::PermissionType permission,
                               const GURL& requesting_origin);
  static void PermissionDismissed(content::PermissionType permission,
                                  const GURL& requesting_origin);
  static void PermissionIgnored(content::PermissionType permission,
                                const GURL& requesting_origin);
  static void PermissionRevoked(content::PermissionType permission,
                                const GURL& revoked_origin);

  // UMA specifically for when permission prompts are shown. This should be
  // roughly equivalent to the metrics above, however it is
  // useful to have separate UMA to a few reasons:
  // - to account for, and get data on coalesced permission bubbles
  // - there are other types of permissions prompts (e.g. download limiting)
  //   which don't go through PermissionContext
  // - the above metrics don't always add up (e.g. sum of
  //   granted+denied+dismissed+ignored is not equal to requested), so it is
  //   unclear from those metrics alone how many prompts are seen by users.
  static void PermissionPromptShown(
      const std::vector<PermissionBubbleRequest*>& requests);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionUmaUtil);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_
