// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/tabs/base_tab.h"
#include "ui/gfx/point.h"

namespace ui {
class MultiAnimation;
}

///////////////////////////////////////////////////////////////////////////////
//
// TabRenderer
//
//  A View that renders a Tab, either in a TabStrip or in a DraggedTabView.
//
///////////////////////////////////////////////////////////////////////////////
class Tab : public BaseTab {
 public:
  // The menu button's class name.
  static const char kViewClassName[];

  explicit Tab(TabController* controller);
  virtual ~Tab();

  // Start/stop the mini-tab title animation.
  void StartMiniTabTitleAnimation();
  void StopMiniTabTitleAnimation();

  // Set the background offset used to match the image in the inactive tab
  // to the frame image.
  void set_background_offset(const gfx::Point& offset) {
    background_offset_ = offset;
  }

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();
  // Returns the minimum possible size of a selected Tab. Selected tabs must
  // always show a close button and have a larger minimum size than unselected
  // tabs.
  static gfx::Size GetMinimumSelectedSize();
  // Returns the preferred size of a single Tab, assuming space is
  // available.
  static gfx::Size GetStandardSize();

  // Returns the width for touch tabs.
  static int GetTouchWidth();

  // Returns the width for mini-tabs. Mini-tabs always have this width.
  static int GetMiniWidth();

 protected:
  // BaseTab overrides:
  virtual const gfx::Rect& GetTitleBounds() const OVERRIDE;
  virtual const gfx::Rect& GetIconBounds() const OVERRIDE;
  virtual void DataChanged(const TabRendererData& old) OVERRIDE;

  // Returns whether the Tab should display a close button.
  virtual bool ShouldShowCloseBox() const;

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual bool HasHitTestMask() const OVERRIDE;
  virtual void GetHitTestMask(gfx::Path* path) const OVERRIDE;
  virtual bool GetTooltipTextOrigin(const gfx::Point& p,
                                    gfx::Point* origin) const OVERRIDE;
  virtual void OnMouseMoved(const views::MouseEvent& event) OVERRIDE;

  // Paint various portions of the Tab
  gfx::ImageSkia* GetTabBackgroundImage(chrome::search::Mode::Type mode) const;
  void PaintTabBackground(gfx::Canvas* canvas);
  void PaintInactiveTabBackgroundWithTitleChange(gfx::Canvas* canvas);
  void PaintInactiveTabBackground(gfx::Canvas* canvas);
  void PaintActiveTabBackground(gfx::Canvas* canvas,
                                gfx::ImageSkia* tab_background);

  // Returns the number of favicon-size elements that can fit in the tab's
  // current size.
  int IconCapacity() const;

  // Returns whether the Tab should display a favicon.
  bool ShouldShowIcon() const;

  // Gets the throb value for the tab. When a tab is not selected the
  // active background is drawn at |GetThrobValue()|%. This is used for hover,
  // mini tab title change and pulsing.
  double GetThrobValue();

  // Performs a one-time initialization of static resources such as tab images.
  static void InitTabResources();

  // Returns the minimum possible size of a single unselected Tab, not
  // including considering touch mode.
  static gfx::Size GetBasicMinimumUnselectedSize();

  // Loads the images to be used for the tab background.
  static void LoadTabImages();

  // The bounds of various sections of the display.
  gfx::Rect favicon_bounds_;
  gfx::Rect title_bounds_;

  // The offset used to paint the inactive background image.
  gfx::Point background_offset_;

  // Animation used when the title of an inactive mini tab changes.
  scoped_ptr<ui::MultiAnimation> mini_title_animation_;

  struct TabImage {
    gfx::ImageSkia* image_l;
    gfx::ImageSkia* image_c;
    gfx::ImageSkia* image_r;
    int l_width;
    int r_width;
    int y_offset;
  };
  static TabImage tab_active_;
  static TabImage tab_active_search_;
  static TabImage tab_inactive_;
  static TabImage tab_alpha_;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_;

  // The current color of the close button.
  SkColor close_button_color_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
