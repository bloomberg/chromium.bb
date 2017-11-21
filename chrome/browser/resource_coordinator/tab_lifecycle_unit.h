// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_

#include "chrome/browser/resource_coordinator/lifecycle_unit.h"

namespace content {
class RenderProcessHost;
class WebContents;
}  // namespace content

namespace resource_coordinator {

// Represents a tab.
class TabLifecycleUnit : public LifecycleUnit {
 public:
  // Returns the TabLifecycleUnit associated with |web_contents|, or nullptr if
  // the WebContents is not associated with a tab.
  static TabLifecycleUnit* FromWebContents(content::WebContents* web_contents);

  // Returns the WebContents associated with this tab.
  virtual content::WebContents* GetWebContents() const = 0;

  // Returns the RenderProcessHost associated with this tab.
  virtual content::RenderProcessHost* GetRenderProcessHost() const = 0;

  // Returns true if the tab is playing audio, has played audio recently, is
  // accessing the microphone, is accessing the camera or is being mirrored.
  virtual bool IsMediaTab() const = 0;

  // Returns true if this tab can be automatically discarded.
  virtual bool IsAutoDiscardable() const = 0;

  // Allows/disallows this tab to be automatically discarded.
  virtual void SetAutoDiscardable(bool auto_discardable) = 0;

 protected:
  // Sets the TabLifecycleUnit associated with |web_contents|.
  static void SetForWebContents(content::WebContents* web_contents,
                                TabLifecycleUnit* tab_lifecycle_unit);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_H_
