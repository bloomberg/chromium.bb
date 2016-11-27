// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEB_APP_LEFT_HEADER_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEB_APP_LEFT_HEADER_VIEW_ASH_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"

class BrowserView;

namespace ash {
class FrameCaptionButton;
}

// WebAppLeftHeaderView is a container view for any icons on the left of the
// frame used for web app style windows. It contains a back button and a
// location icon.
class WebAppLeftHeaderView : public views::View,
                             public views::ButtonListener {
 public:
  static const char kViewClassName[];

  explicit WebAppLeftHeaderView(BrowserView* browser_view);
  ~WebAppLeftHeaderView() override;

  // Updates the view.
  void Update();

  // Update whether to paint the header view as active or not.
  void SetPaintAsActive(bool active);

  views::View* GetLocationIconView() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppLeftHeaderViewTest, BackButton);
  FRIEND_TEST_ALL_PREFIXES(WebAppLeftHeaderViewTest, LocationIcon);

  // views::View:
  const char* GetClassName() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Ask the browser to show the website settings dialog.
  void ShowWebsiteSettings() const;

  // Update the state of the back button.
  void UpdateBackButtonState(bool enabled);

  // The BrowserView for the frame.
  BrowserView* browser_view_;

  // The back button.
  ash::FrameCaptionButton* back_button_;

  // The location icon indicator. Shows the connection security status and
  // allows the user to bring up the website settings dialog.
  ash::FrameCaptionButton* location_icon_;

  DISALLOW_COPY_AND_ASSIGN(WebAppLeftHeaderView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEB_APP_LEFT_HEADER_VIEW_ASH_H_
