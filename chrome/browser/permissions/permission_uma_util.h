// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_

#include <vector>

#include "base/logging.h"
#include "base/macros.h"

enum class PermissionRequestGestureType;
class GURL;
class Profile;

namespace content {
enum class PermissionType;
}  // namespace content

class PermissionRequest;

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

// This should stay in sync with the SourceUI enum in the permission report
// protobuf (src/chrome/common/safe_browsing/permission_report.proto).
enum class PermissionSourceUI {
  PROMPT = 0,
  OIB = 1,
  SITE_SETTINGS = 2,
  PAGE_ACTION = 3,

  // Always keep this at the end.
  SOURCE_UI_NUM,
};

// Provides a convenient way of logging UMA for permission related operations.
class PermissionUmaUtil {
 public:
  static const char kPermissionsPromptShown[];
  static const char kPermissionsPromptShownGesture[];
  static const char kPermissionsPromptShownNoGesture[];
  static const char kPermissionsPromptAccepted[];
  static const char kPermissionsPromptAcceptedGesture[];
  static const char kPermissionsPromptAcceptedNoGesture[];
  static const char kPermissionsPromptDenied[];
  static const char kPermissionsPromptDeniedGesture[];
  static const char kPermissionsPromptDeniedNoGesture[];
  static const char kPermissionsPromptRequestsPerPrompt[];
  static const char kPermissionsPromptMergedBubbleTypes[];
  static const char kPermissionsPromptMergedBubbleAccepted[];
  static const char kPermissionsPromptMergedBubbleDenied[];

  static void PermissionRequested(content::PermissionType permission,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  Profile* profile);
  static void PermissionGranted(content::PermissionType permission,
                                PermissionRequestGestureType gesture_type,
                                const GURL& requesting_origin,
                                Profile* profile);
  static void PermissionDenied(content::PermissionType permission,
                               PermissionRequestGestureType gesture_type,
                               const GURL& requesting_origin,
                               Profile* profile);
  static void PermissionDismissed(content::PermissionType permission,
                                  PermissionRequestGestureType gesture_type,
                                  const GURL& requesting_origin,
                                  Profile* profile);
  static void PermissionIgnored(content::PermissionType permission,
                                PermissionRequestGestureType gesture_type,
                                const GURL& requesting_origin,
                                Profile* profile);
  static void PermissionRevoked(content::PermissionType permission,
                                PermissionSourceUI source_ui,
                                const GURL& revoked_origin,
                                Profile* profile);

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
      const std::vector<PermissionRequest*>& requests);

  // The following two functions can be combined with the PermissionPromptShown
  // metrics to calculate accept, deny and ignore rates.
  // Note that for coalesced permission bubbles, PermissionPromptAccepted will
  // always be called, with |accept_states| containing whether each request was
  // accepted or denied.
  static void PermissionPromptAccepted(
      const std::vector<PermissionRequest*>& requests,
      const std::vector<bool>& accept_states);

  static void PermissionPromptDenied(
      const std::vector<PermissionRequest*>& requests);

 private:
  friend class PermissionUmaUtilTest;

  static bool IsOptedIntoPermissionActionReporting(Profile* profile);

  static void RecordPermissionAction(content::PermissionType permission,
                                     PermissionAction action,
                                     PermissionSourceUI source_ui,
                                     PermissionRequestGestureType gesture_type,
                                     const GURL& requesting_origin,
                                     Profile* profile);

  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionUmaUtil);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_
