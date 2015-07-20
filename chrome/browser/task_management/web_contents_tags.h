// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_WEB_CONTENTS_TAGS_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_WEB_CONTENTS_TAGS_H_

#include "base/macros.h"

class BackgroundContents;
class Panel;

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

  // Tag a WebContents owned by the PrerenderManager so that it shows up in the
  // task manager. Calling this function creates a PrerenderTag, and attaches it
  // to |web_contents|. If an instance is already attached, this does nothing.
  // The resulting tag does not have to be cleaned up by the caller, as it is
  // owned by |web_contents|.
  static void CreateForPrerenderContents(content::WebContents* web_contents);

  // Tag a WebContents owned by the TabStripModel so that it shows up in the
  // task manager. Calling this function creates a TabContentsTag, and attaches
  // it to |web_contents|. If an instance is already attached, this does
  // nothing. The resulting tag does not have to be cleaned up by the caller, as
  // it is owned by |web_contents|.
  static void CreateForTabContents(content::WebContents* web_contents);

  // Tag a WebContents owned by a |panel| in the PanelManager so that it shows
  // up in the task manager. Calling this function creates a PanelTag, and
  // attaches it to |web_contents|. If an instance is already attached, this
  // does nothing. The resulting tag does not have to be cleaned up by the
  // caller, as it is owned by |web_contents|.
  // Note: |web_contents| must be equal to |panel->GetWebContents()|.
  static void CreateForPanel(content::WebContents* web_contents, Panel* panel);

  // Clears the task-manager tag, created by any of the above functions, from
  // the given |web_contents| if any.
  // Clearing the tag is necessary only when you need to re-tag an existing
  // WebContents, to indicate a change in ownership.
  static void ClearTag(content::WebContents* web_contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsTags);
};

}  // namespace task_management


#endif  // CHROME_BROWSER_TASK_MANAGEMENT_WEB_CONTENTS_TAGS_H_
