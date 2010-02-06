// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_H_
#define CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_H_

#include "app/gfx/font.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/view.h"

class SideTab;

class SideTabModel {
 public:
  // Returns metadata about the specified |tab|.
  virtual string16 GetTitle(SideTab* tab) const = 0;
  virtual SkBitmap GetIcon(SideTab* tab) const = 0;
  virtual bool IsSelected(SideTab* tab) const = 0;
};

class SideTab : public views::View {
 public:
  explicit SideTab(SideTabModel* model);
  virtual ~SideTab();

  // views::View Overrides:
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();

 private:
  // Loads class-specific resources.
  static void InitClass();

  SideTabModel* model_;

  static gfx::Font* font_;

  DISALLOW_COPY_AND_ASSIGN(SideTab);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_H_
