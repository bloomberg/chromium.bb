// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MANAGE_PASSWORDS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MANAGE_PASSWORDS_ICON_VIEW_H_

#include "chrome/browser/ui/passwords/manage_passwords_icon_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/views/controls/image_view.h"

class ManagePasswordsIconController;

// View for the password icon in the Omnibox.
class ManagePasswordsIconView : public views::ImageView {
 public:
  // Clicking on the ManagePasswordsIconView shows a ManagePasswordsBubbleView,
  // which requires the current WebContents. Because the current WebContents
  // changes as the user switches tabs, it cannot be provided in the
  // constructor. Instead, a LocationBarView::Delegate is passed here so that it
  // can be queried for the current WebContents as needed.
  explicit ManagePasswordsIconView(
      LocationBarView::Delegate* location_bar_delegate);
  virtual ~ManagePasswordsIconView();

  // Updates the image and its tooltip appropriately, hiding or showing the icon
  // as needed.
  void Update(ManagePasswordsIconController* manage_passwords_icon_controller);

  // Shows a bubble from the icon if a password form was submitted.
  void ShowBubbleIfNeeded(
      ManagePasswordsIconController* manage_passwords_icon_controller);

 private:
  // views::ImageView:
  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;

  // The delegate used to get the currently visible WebContents.
  LocationBarView::Delegate* location_bar_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MANAGE_PASSWORDS_ICON_VIEW_H_
