// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_DELEGATE_H_

namespace content {
class WebContents;
}
class BrowserWindow;

class ConstrainedWindowTabHelperDelegate {
 public:
  // Returns true if constrained windows should be focused. Default is true.
  virtual bool ShouldFocusConstrainedWindow();

  // Changes the blocked state of |tab_contents|. WebContentses are considered
  // blocked while displaying a tab modal dialog. During that time renderer host
  // will ignore any UI interaction within WebContents outside of the
  // currently displaying dialog.
  virtual void SetTabContentBlocked(content::WebContents* web_contents,
                                    bool blocked);

  // Returns the window for this Browser.
  virtual BrowserWindow* GetBrowserWindow();

 protected:
  virtual ~ConstrainedWindowTabHelperDelegate();
};

#endif  // CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_DELEGATE_H_
