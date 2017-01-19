// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/permission_type.h"
#include "url/gurl.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

namespace base {
class Time;
}

class HostContentSettingsMap;

// The PermissionDecisionAutoBlocker decides whether or not a given origin
// should be automatically blocked from requesting a permission. When an origin
// is blocked, it is placed under an "embargo". Until the embargo expires, any
// requests made by the origin are automatically blocked. Once the embargo is
// lifted, the origin will be permitted to request a permission again, which may
// result in it being placed under embargo again. Currently, an origin can be
// placed under embargo if it appears on Safe Browsing's API blacklist, or if it
// has a number of prior dismissals greater than a threshold.
class PermissionDecisionAutoBlocker {
 public:
  // Removes any recorded counts for urls which match |filter| under |profile|.
  static void RemoveCountsByUrl(Profile* profile,
                                base::Callback<bool(const GURL& url)> filter);

  // Returns the current number of dismisses recorded for |permission| type at
  // |url|.
  static int GetDismissCount(const GURL& url,
                             content::PermissionType permission,
                             Profile* profile);

  // Returns the current number of ignores recorded for |permission|
  // type at |url|.
  static int GetIgnoreCount(const GURL& url,
                            content::PermissionType permission,
                            Profile* profile);

  // Records that a dismissal of a prompt for |permission| was made. If the
  // total number of dismissals exceeds a threshhold and
  // features::kBlockPromptsIfDismissedOften is enabled it will place |url|
  // under embargo for |permission|.
  static bool RecordDismissAndEmbargo(const GURL& url,
                                      content::PermissionType permission,
                                      Profile* profile,
                                      base::Time current_time);

  // Records that an ignore of a prompt for |permission| was made.
  static int RecordIgnore(const GURL& url,
                          content::PermissionType permission,
                          Profile* profile);

  // Records that a dismissal of a prompt for |permission| was made, and returns
  // true if this dismissal should be considered a block. False otherwise.
  // TODO(meredithl): Remove in favour of embargoing on repeated dismissals.
  static bool ShouldChangeDismissalToBlock(const GURL& url,
                                           content::PermissionType permission,
                                           Profile* profile);

  // Updates the threshold to start blocking prompts from the field trial.
  static void UpdateFromVariations();

  // Checks if |request_origin| is under embargo for |permission|. Internally,
  // this will make a call to IsUnderEmbargo to check the content setting first,
  // but may also make a call to Safe Browsing to check if |request_origin| is
  // blacklisted for |permission|, which is performed asynchronously.
  static void UpdateEmbargoedStatus(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> db_manager,
      content::PermissionType permission,
      const GURL& request_origin,
      content::WebContents* web_contents,
      int timeout,
      Profile* profile,
      base::Time current_time,
      base::Callback<void(bool)> callback);

  // Checks the status of the content setting to determine if |request_origin|
  // is under embargo for |permission|. This checks both embargo for Permissions
  // Blacklisting and repeated dismissals.
  static bool IsUnderEmbargo(content::PermissionType permission,
                             Profile* profile,
                             const GURL& request_origin,
                             base::Time current_time);

 private:
  friend class PermissionContextBaseTests;
  friend class PermissionDecisionAutoBlockerUnitTest;

  static void CheckSafeBrowsingResult(content::PermissionType permission,
                                      Profile* profile,
                                      const GURL& request_origin,
                                      base::Time current_time,
                                      base::Callback<void(bool)> callback,
                                      bool should_be_embargoed);

  static void PlaceUnderEmbargo(content::PermissionType permission,
                                const GURL& request_origin,
                                HostContentSettingsMap* map,
                                base::Time current_time,
                                const char* key);

  // Keys used for storing count data in a website setting.
  static const char kPromptDismissCountKey[];
  static const char kPromptIgnoreCountKey[];
  static const char kPermissionDismissalEmbargoKey[];
  static const char kPermissionBlacklistEmbargoKey[];

  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionDecisionAutoBlocker);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_
