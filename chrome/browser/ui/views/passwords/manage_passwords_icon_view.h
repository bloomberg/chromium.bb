// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/views/controls/image_view.h"

class ManagePasswordsBubbleUIController;

// View for the password icon in the Omnibox.
class ManagePasswordsIconView : public ManagePasswordsIcon,
                                public views::ImageView {
 public:
  // Clicking on the ManagePasswordsIconView shows a ManagePasswordsBubbleView,
  // which requires the current WebContents. Because the current WebContents
  // changes as the user switches tabs, it cannot be provided in the
  // constructor. Instead, a LocationBarView::Delegate is passed here so that it
  // can be queried for the current WebContents as needed.
  explicit ManagePasswordsIconView(
      LocationBarView::Delegate* location_bar_delegate);
  virtual ~ManagePasswordsIconView();

  // ManagePasswordsIcon:
  virtual void ShowBubbleWithoutUserInteraction() OVERRIDE;

 protected:
  // ManagePasswordsIcon:
  virtual void SetStateInternal(ManagePasswordsIcon::State state) OVERRIDE;

 private:
  // views::ImageView:
  virtual bool GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;

  // The delegate used to get the currently visible WebContents.
  LocationBarView::Delegate* location_bar_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_
