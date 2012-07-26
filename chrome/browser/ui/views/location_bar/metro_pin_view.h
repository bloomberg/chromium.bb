// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_METRO_PIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_METRO_PIN_VIEW_H_

#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"
#include "ui/views/controls/button/image_button.h"

class CommandUpdater;

class MetroPinView
    : public views::ImageButton,
      public views::ButtonListener,
      public TouchableLocationBarView {
 public:
  explicit MetroPinView(CommandUpdater* command_updater);
  virtual ~MetroPinView();

  // When the page is already pinned, clicking the pin view will cause the page
  // to become unpinned.
  void SetIsPinned(bool is_pinned);

  // views::ButtonListener.
  virtual void ButtonPressed(Button* sender,
                             const views::Event& event) OVERRIDE;

  // TouchableLocationBarView.
  virtual int GetBuiltInHorizontalPadding() const OVERRIDE;

 private:
  // The CommandUpdater for the Browser object that owns the location bar.
  CommandUpdater* command_updater_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MetroPinView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_METRO_PIN_VIEW_H_
