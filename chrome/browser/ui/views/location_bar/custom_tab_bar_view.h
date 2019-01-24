// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CUSTOM_TAB_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CUSTOM_TAB_BAR_VIEW_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
class Rect;
}

class CustomTabBarTitleOriginView;
class BrowserView;

// A CustomTabBarView displays a read only title and origin for the current page
// and a security status icon. This is visible if the hosted app window is
// displaying a page over HTTP or if the current page is outside of the app
// scope.
class CustomTabBarView : public views::View,
                         public TabStripModelObserver,
                         public LocationIconView::Delegate,
                         public views::ButtonListener {
 public:
  static const char kViewClassName[];

  CustomTabBarView(BrowserView* browser_view,
                   LocationBarView::Delegate* delegate);
  ~CustomTabBarView() override;

  LocationIconView* location_icon_view() { return location_icon_view_; }

  // views::View:
  gfx::Rect GetAnchorBoundsInScreen() const override;

  // TabstripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // LocationIconView::Delegate:
  content::WebContents* GetWebContents() override;
  bool IsEditingOrEmpty() override;
  void OnLocationIconPressed(const ui::MouseEvent& event) override;
  void OnLocationIconDragged(const ui::MouseEvent& event) override;
  SkColor GetSecurityChipColor(
      security_state::SecurityLevel security_level) const override;
  bool ShowPageInfoDialog() override;
  const LocationBarModel* GetLocationBarModel() const override;
  gfx::ImageSkia GetLocationIcon(LocationIconView::Delegate::IconFetchedCallback
                                     on_icon_fetched) const override;
  SkColor GetLocationIconInkDropColor() const override;

  // ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Methods for testing.
  base::string16 title_for_testing() const { return last_title_; }
  base::string16 location_for_testing() const { return last_location_; }

 private:
  SkColor title_bar_color_;

  base::string16 last_title_;
  base::string16 last_location_;

  views::Button* close_button_ = nullptr;
  LocationBarView::Delegate* delegate_ = nullptr;
  LocationIconView* location_icon_view_ = nullptr;
  CustomTabBarTitleOriginView* title_origin_view_ = nullptr;
  ScopedObserver<TabStripModel, CustomTabBarView> tab_strip_model_observer_;

  DISALLOW_COPY_AND_ASSIGN(CustomTabBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CUSTOM_TAB_BAR_VIEW_H_
