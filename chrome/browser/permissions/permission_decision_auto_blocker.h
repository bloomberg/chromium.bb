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

  explicit PermissionDecisionAutoBlocker(Profile* profile);

  // Records that an ignore of a prompt for |permission| was made.
  int RecordIgnore(const GURL& url, content::PermissionType permission);

  // Records that a dismissal of a prompt for |permission| was made, and returns
  // true if this dismissal should be considered a block. False otherwise.
  bool ShouldChangeDismissalToBlock(const GURL& url,
                                    content::PermissionType permission);

 private:
  friend class PermissionContextBaseTests;
  friend class PermissionDecisionAutoBlockerUnitTest;
  friend class RemovePermissionPromptCountsTest;

  // Keys used for storing count data in a website setting.
  static const char kPromptDismissCountKey[];
  static const char kPromptIgnoreCountKey[];

  // Returns the current number of actions recorded under |key| for |permission|
  // type at |url|.
  int GetActionCountForTest(const GURL& url,
                            content::PermissionType permission,
                            const char* key);

  // Records that the user performed an action for a prompt of type |permission|
  // on |url| to a website setting keyed by |key|. Returns the total number of
  // |key| actions which have been performed for |url|.
  int RecordActionInWebsiteSettings(const GURL& url,
                                    content::PermissionType permission,
                                    const char* key);

  // Updates |prompt_dismissals_before_block_|.
  void UpdateFromVariations();

  Profile *profile_;
  int prompt_dismissals_before_block_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_H_
