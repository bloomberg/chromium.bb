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
// web_contents that are owned by various types of services.
//
// Any service that creates WebContents instances (via WebContents::Create)
// needs to make sure that they are tagged using this mechanism, otherwise the
// associated render processes will not show up in the task manager.
class WebContentsTags {
 public:
  // Creates a BackgroundContentsTag, and attaches it to the specified
  // WebContents. If an instance is already attached, it does nothing.
  static void CreateForBackgroundContents(
      content::WebContents* web_contents,
      BackgroundContents* background_contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsTags);
};

}  // namespace task_management


#endif  // CHROME_BROWSER_TASK_MANAGEMENT_WEB_CONTENTS_TAGS_H_
