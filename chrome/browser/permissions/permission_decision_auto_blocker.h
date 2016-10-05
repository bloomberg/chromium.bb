// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/public/browser/permission_type.h"
#include "url/gurl.h"

class GURL;
class Profile;

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

  // Records that a dismissal of a prompt for |permission| was made.
  static int RecordDismiss(const GURL& url,
                           content::PermissionType permission,
                           Profile* profile);

  // Records that an ignore of a prompt for |permission| was made.
  static int RecordIgnore(const GURL& url,
                          content::PermissionType permission,
                          Profile* profile);

  // Records that a dismissal of a prompt for |permission| was made, and returns
  // true if this dismissal should be considered a block. False otherwise.
  static bool ShouldChangeDismissalToBlock(const GURL& url,
                                           content::PermissionType permission,
                                           Profile* profile);

  // Updates the threshold to start blocking prompts from the field trial.
  static void UpdateFromVariations();

 private:
  friend class PermissionContextBaseTests;

  // Keys used for storing count data in a website setting.
  static const char kPromptDismissCountKey[];
  static const char kPromptIgnoreCountKey[];

  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionDecisionAutoBlocker);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_
