// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_DELEGATE_H_
#pragma once

class TabContentsWrapper;

class ConstrainedWindowTabHelperDelegate {
 public:
  // Invoked prior to the TabContentsWrapper showing a constrained window.
  virtual void WillShowConstrainedWindow(TabContentsWrapper* source);

  // Returns true if constrained windows should be focused. Default is true.
  virtual bool ShouldFocusConstrainedWindow();

  // Changes the blocked state of |wrapper|. TabContents are considered blocked
  // while displaying a tab modal dialog. During that time renderer host will
  // ignore any UI interaction within TabContent outside of the currently
  // displaying dialog.
  virtual void SetTabContentBlocked(TabContentsWrapper* wrapper, bool blocked);

 protected:
  virtual ~ConstrainedWindowTabHelperDelegate();
};

#endif  // CHROME_BROWSER_UI_CONSTRAINED_WINDOW_TAB_HELPER_DELEGATE_H_
