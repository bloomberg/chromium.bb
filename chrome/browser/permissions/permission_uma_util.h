// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/permissions/permission_util.h"
#include "content/public/browser/permission_type.h"

namespace content {
class WebContents;
}

enum class PermissionRequestGestureType;
class GURL;
class PermissionRequest;
class Profile;

// This should stay in sync with the SourceUI enum in the permission report
// protobuf (src/chrome/common/safe_browsing/permission_report.proto).
enum class PermissionSourceUI {
  PROMPT = 0,
  OIB = 1,
  SITE_SETTINGS = 2,
  PAGE_ACTION = 3,

  // Always keep this at the end.
  NUM,
};

// This should stay in sync with the PersistDecision enum in the permission
// report message (src/chrome/common/safe_browsing/permission_report.proto).
enum class PermissionPersistDecision {
  UNSPECIFIED = 0,
  PERSISTED = 1,
  NOT_PERSISTED = 2,
};

// Any new values should be inserted immediately prior to NUM.
enum class SafeBrowsingResponse {
  NOT_BLACKLISTED = 0,
  TIMEOUT = 1,
  BLACKLISTED = 2,

  // Always keep this at the end.
  NUM,
};

// Any new values should be inserted immediately prior to NUM.
enum class PermissionEmbargoStatus {
  NOT_EMBARGOED = 0,
  PERMISSIONS_BLACKLISTING = 1,
  REPEATED_DISMISSALS = 2,
  REPEATED_IGNORES = 3,

  // Keep this at the end.
  NUM,
};

// A bundle for the information sent in a PermissionReport.
struct PermissionReportInfo {
  PermissionReportInfo(
      const GURL& origin,
      ContentSettingsType permission,
      PermissionAction action,
      PermissionSourceUI source_ui,
      PermissionRequestGestureType gesture_type,
      PermissionPersistDecision persist_decision,
      int num_prior_dismissals,
      int num_prior_ignores);

  PermissionReportInfo(const PermissionReportInfo& other);

  GURL origin;
  ContentSettingsType permission;
  PermissionAction action;
  PermissionSourceUI source_ui;
  PermissionRequestGestureType gesture_type;
  PermissionPersistDecision persist_decision;
  int num_prior_dismissals;
  int num_prior_ignores;
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
  static const char kPermissionsPromptAcceptedPriorDismissCountPrefix[];
  static const char kPermissionsPromptAcceptedPriorIgnoreCountPrefix[];
  static const char kPermissionsPromptDeniedPriorDismissCountPrefix[];
  static const char kPermissionsPromptDeniedPriorIgnoreCountPrefix[];
  static const char kPermissionsPromptDismissedPriorDismissCountPrefix[];
  static const char kPermissionsPromptDismissedPriorIgnoreCountPrefix[];
  static const char kPermissionsPromptIgnoredPriorDismissCountPrefix[];
  static const char kPermissionsPromptIgnoredPriorIgnoreCountPrefix[];

  static void PermissionRequested(ContentSettingsType permission,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  Profile* profile);
  static void PermissionGranted(ContentSettingsType permission,
                                PermissionRequestGestureType gesture_type,
                                const GURL& requesting_origin,
                                Profile* profile);
  static void PermissionDenied(ContentSettingsType permission,
                               PermissionRequestGestureType gesture_type,
                               const GURL& requesting_origin,
                               Profile* profile);
  static void PermissionDismissed(ContentSettingsType permission,
                                  PermissionRequestGestureType gesture_type,
                                  const GURL& requesting_origin,
                                  Profile* profile);
  static void PermissionIgnored(ContentSettingsType permission,
                                PermissionRequestGestureType gesture_type,
                                const GURL& requesting_origin,
                                Profile* profile);
  static void PermissionRevoked(ContentSettingsType permission,
                                PermissionSourceUI source_ui,
                                const GURL& revoked_origin,
                                Profile* profile);

  static void RecordEmbargoPromptSuppression(
      PermissionEmbargoStatus embargo_status);

  static void RecordEmbargoPromptSuppressionFromSource(
      PermissionStatusSource source);

  static void RecordEmbargoStatus(PermissionEmbargoStatus embargo_status);

  static void RecordSafeBrowsingResponse(base::TimeDelta response_time,
                                         SafeBrowsingResponse response);

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

  static void PermissionPromptResolved(
      const std::vector<PermissionRequest*>& requests,
      const content::WebContents* web_contents,
      PermissionAction permission_action);

  // Records the request type and gesture type for a shown, accepted, and denied
  // prompt. Defined separately as Android must call this method explicitly
  // until the removal of PermissionQueueController is completed.
  static void RecordPermissionPromptShown(
      PermissionRequestType request_type,
      PermissionRequestGestureType gesture_type);

  static void RecordPermissionPromptAccepted(
      PermissionRequestType request_type,
      PermissionRequestGestureType gesture_type);

  static void RecordPermissionPromptDenied(
      PermissionRequestType request_type,
      PermissionRequestGestureType gesture_type);

  // A permission prompt was accepted or denied, and the prompt displayed a
  // persistence toggle. Records whether the toggle was enabled (persist) or
  // disabled (don't persist).
  static void PermissionPromptAcceptedWithPersistenceToggle(
      ContentSettingsType permission,
      bool toggle_enabled);

  static void PermissionPromptDeniedWithPersistenceToggle(
      ContentSettingsType permission,
      bool toggle_enabled);

  static void RecordWithBatteryBucket(const std::string& histogram);

  // Permission Action Reporting data is only sent in official, Chrome branded
  // builds. This function allows this to be overridden for testing.
  static void FakeOfficialBuildForTest();

 private:
  friend class PermissionUmaUtilTest;

  static bool IsOptedIntoPermissionActionReporting(Profile* profile);

  static void RecordPermissionAction(ContentSettingsType permission,
                                     PermissionAction action,
                                     PermissionSourceUI source_ui,
                                     PermissionRequestGestureType gesture_type,
                                     const GURL& requesting_origin,
                                     Profile* profile);

  // Records |count| total prior actions for a prompt of type |permission|
  // for a single origin using |prefix| for the metric.
  static void RecordPermissionPromptPriorCount(
      ContentSettingsType permission,
      const std::string& prefix,
      int count);

  static void RecordPromptDecided(
      const std::vector<PermissionRequest*>& requests,
      bool accepted);

  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionUmaUtil);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UMA_UTIL_H_
