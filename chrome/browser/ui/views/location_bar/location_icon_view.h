// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_

#include "chrome/browser/ui/views/location_bar/page_info_helper.h"
#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"
#include "ui/views/controls/image_view.h"

class LocationBarView;
namespace views {
class MouseEvent;
}

// LocationIconView is used to display an icon to the left of the edit field.
// This shows the user's current action while editing, the page security
// status on https pages, or a globe for other URLs.
class LocationIconView
    : public views::ImageView,
      public TouchableLocationBarView {
 public:
  explicit LocationIconView(LocationBarView* location_bar);
  virtual ~LocationIconView();

  // Overridden from views::ImageView:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual ui::EventResult OnGestureEvent(
      const ui::GestureEvent& event) OVERRIDE;

  // TouchableLocationBarView.
  virtual int GetBuiltInHorizontalPadding() const OVERRIDE;

  // Whether we should show the tooltip for this icon or not.
  void ShowTooltip(bool show);

 private:
  PageInfoHelper page_info_helper_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LocationIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_ICON_VIEW_H_
