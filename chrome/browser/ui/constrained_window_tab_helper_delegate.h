// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_DELEGATE_H_
#pragma once

class TabContents;

class ConstrainedWindowTabHelperDelegate {
 public:
  // Invoked prior to the TabContents showing a constrained window.
  virtual void WillShowConstrainedWindow(TabContents* source);

  // Returns true if constrained windows should be focused. Default is true.
  virtual bool ShouldFocusConstrainedWindow();

  // Changes the blocked state of |tab_contents|. TabContentses are considered
  // blocked while displaying a tab modal dialog. During that time renderer host
  // will ignore any UI interaction within TabContents outside of the
  // currently displaying dialog.
  virtual void SetTabContentBlocked(TabContents* tab_contents, bool blocked);

 protected:
  virtual ~ConstrainedWindowTabHelperDelegate();
};

#endif  // CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_DELEGATE_H_
