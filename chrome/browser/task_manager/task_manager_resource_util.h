// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_RESOURCE_UTIL_H_
#define CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_RESOURCE_UTIL_H_

#include "base/basictypes.h"
#include "base/string16.h"

class Profile;

namespace content {
class WebContents;
}

class TaskManagerResourceUtil {
 public:
  // Returns the appropriate message prefix ID for tabs and extensions,
  // reflecting whether they are apps or in incognito mode.
  static int GetMessagePrefixID(bool is_app,
                                bool is_extension,
                                bool is_incognito,
                                bool is_prerender,
                                bool is_instant_overlay,
                                bool is_background);

  // Returns the name of profle from InfoCache.
  static string16 GetProfileNameFromInfoCache(Profile* profile);

  // Returns the title from web contents.
  static string16 GetTitleFromWebContents(content::WebContents* web_contents);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TaskManagerResourceUtil);
};

#endif  // CHROME_BROWSER_TASK_MANAGER_TASK_MANAGER_RESOURCE_UTIL_H_
