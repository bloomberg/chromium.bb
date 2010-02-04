// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_
#define CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_

#include "views/view.h"

class Profile;

class SideTabStrip : public views::View {
 public:
  SideTabStrip();
  virtual ~SideTabStrip();

  // Whether or not the browser has been run with the "enable-vertical-tabs"
  // command line flag that allows the SideTabStrip to be optionally shown.
  static bool Available();

  // Whether or not the vertical tabstrip is shown. Only valid if Available()
  // returns true.
  static bool Visible(Profile* profile);

  // views::View overrides:
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();

 private:
  DISALLOW_COPY_AND_ASSIGN(SideTabStrip);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_STRIP_H_
