// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CHROME_TO_MOBILE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CHROME_TO_MOBILE_VIEW_H_
#pragma once

#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"
#include "ui/views/controls/image_view.h"

class LocationBarView;

namespace views {
class KeyEvent;
class MouseEvent;
}

// A Page Action image view for the Chrome To Mobile bubble.
class ChromeToMobileView : public views::ImageView,
                           public CommandUpdater::CommandObserver,
                           public TouchableLocationBarView {
 public:
  ChromeToMobileView(LocationBarView* location_bar_view,
                     CommandUpdater* command_updater);
  virtual ~ChromeToMobileView();

  // CommandUpdater::CommandObserver overrides:
  virtual void EnabledStateChangedForCommand(int id, bool enabled) OVERRIDE;

  // TouchableLocationBarView.
  virtual int GetBuiltInHorizontalPadding() const OVERRIDE;

 private:
  // views::ImageView overrides:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;

  // The LocationBarView hosting this view.
  LocationBarView* location_bar_view_;

  // The CommandUpdater for the Browser object that owns the location bar.
  CommandUpdater* command_updater_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChromeToMobileView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CHROME_TO_MOBILE_VIEW_H_
