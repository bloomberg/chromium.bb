// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_WEB_CONTENTS_TAGS_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_WEB_CONTENTS_TAGS_H_

#include "base/macros.h"

class BackgroundContents;

namespace content {
class WebContents;
}  // namespace content

namespace task_management {

// Defines a factory class for creating the TaskManager-specific Tags for the
// WebContents that are owned by various types of services.
//
// Any service or feature that creates WebContents instances (via
// WebContents::Create) needs to make sure that they are tagged using this
// mechanism, otherwise the associated render processes will not show up in the
// task manager.
class WebContentsTags {
 public:
  // Tag a BackgroundContents so that it shows up in the task manager. Calling
  // this function creates a BackgroundContentsTag, and attaches it to
  // |web_contents|. If an instance is already attached, this does nothing. The
  // resulting tag does not have to be cleaned up by the caller, as it is owned
  // by |web_contents|.
  static void CreateForBackgroundContents(
      content::WebContents* web_contents,
      BackgroundContents* background_contents);

  // Tag a DevTools WebContents so that it shows up in the task manager. Calling
  // this function creates a DevToolsTag, and attaches it to |web_contents|. If
  // an instance is already attached, this does nothing. The resulting tag does
  // not have to be cleaned up by the caller, as it is owned by |web_contents|.
  static void CreateForDevToolsContents(content::WebContents* web_contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsTags);
};

}  // namespace task_management


#endif  // CHROME_BROWSER_TASK_MANAGEMENT_WEB_CONTENTS_TAGS_H_
