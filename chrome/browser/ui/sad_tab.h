// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAD_TAB_H_
#define CHROME_BROWSER_UI_SAD_TAB_H_

#include "base/process_util.h"
#include "chrome/browser/ui/sad_tab_types.h"

namespace content {
class WebContents;
}

namespace chrome {

// Cross-platform interface to show the Sad tab UI.
class SadTab {
 public:
  // Factory function to create the platform specific implementations.
  static SadTab* Create(content::WebContents* web_contents, SadTabKind kind);

  // Returns true if the sad tab should be shown.
  static bool ShouldShow(base::TerminationStatus status);

  virtual ~SadTab() {}

  // Shows the Sad tab.
  virtual void Show() = 0;

  // Closes the Sad tab.
  virtual void Close() = 0;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SAD_TAB_H_
