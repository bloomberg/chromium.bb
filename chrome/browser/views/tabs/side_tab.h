// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_H_
#define CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_H_

#include "app/gfx/font.h"
#include "app/slide_animation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/button/button.h"
#include "views/view.h"

class SideTab;

class SideTabModel {
 public:
  // Returns metadata about the specified |tab|.
  virtual string16 GetTitle(SideTab* tab) const = 0;
  virtual SkBitmap GetIcon(SideTab* tab) const = 0;
  virtual bool IsSelected(SideTab* tab) const = 0;

  // Selects the tab.
  virtual void SelectTab(SideTab* tab) = 0;

  // Closes the tab.
  virtual void CloseTab(SideTab* tab) = 0;
};

class SideTab : public views::View,
                public views::ButtonListener,
                public AnimationDelegate {
 public:
  explicit SideTab(SideTabModel* model);
  virtual ~SideTab();

  // AnimationDelegate implementation:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::View Overrides:
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual bool OnMousePressed(const views::MouseEvent& event);

 private:
  // Loads class-specific resources.
  static void InitClass();

  SideTabModel* model_;

  gfx::Rect icon_bounds_;
  gfx::Rect title_bounds_;

  views::Button* close_button_;

  // Hover animation.
  scoped_ptr<SlideAnimation> hover_animation_;

  static gfx::Font* font_;
  static SkBitmap* close_button_n_;
  static SkBitmap* close_button_m_;
  static SkBitmap* close_button_h_;
  static SkBitmap* close_button_p_;

  DISALLOW_COPY_AND_ASSIGN(SideTab);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_SIDE_TAB_H_
