// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_MULTIPLE_WINDOW_INDICATOR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_MULTIPLE_WINDOW_INDICATOR_BUTTON_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/ui/browser_list.h"

// A multiple window indicator that shows up on the right side of the status
// area when you have more than one tabbed browser window open.
class MultipleWindowIndicatorButton : public StatusAreaButton,
                                      public BrowserList::Observer {
 public:
  explicit MultipleWindowIndicatorButton(StatusAreaButton::Delegate* delegate);
  virtual ~MultipleWindowIndicatorButton();

 private:
  // BrowserList::Observer implementation:
  virtual void OnBrowserAdded(const Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(const Browser* browser) OVERRIDE;

  // Update this button's visibility based on number of tabbed browser windows.
  // That is, show the button when there are more than one tabbed browser and
  // hide otherwise.
  void UpdateVisiblity();

  DISALLOW_COPY_AND_ASSIGN(MultipleWindowIndicatorButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_MULTIPLE_WINDOW_INDICATOR_BUTTON_H_
