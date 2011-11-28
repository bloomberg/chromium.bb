// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELOAD_BUTTON_H__
#define CHROME_BROWSER_UI_VIEWS_RELOAD_BUTTON_H__
#pragma once

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/timer.h"
#include "ui/views/controls/button/image_button.h"

class Browser;
class LocationBarView;

////////////////////////////////////////////////////////////////////////////////
//
// ReloadButton
//
// The reload button in the toolbar, which changes to a stop button when a page
// load is in progress. Trickiness comes from the desire to have the 'stop'
// button not change back to 'reload' if the user's mouse is hovering over it
// (to prevent mis-clicks).
//
////////////////////////////////////////////////////////////////////////////////

class ReloadButton : public views::ToggleImageButton,
                     public views::ButtonListener {
 public:
  enum Mode { MODE_RELOAD = 0, MODE_STOP };

  // The button's class name.
  static const char kViewClassName[];

  ReloadButton(LocationBarView* location_bar, Browser* Browser);
  virtual ~ReloadButton();

  // Ask for a specified button state.  If |force| is true this will be applied
  // immediately.
  void ChangeMode(Mode mode, bool force);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* /* button */,
                             const views::Event& event) OVERRIDE;

  // Overridden from views::View:
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

 private:
  friend class ReloadButtonTest;

  void OnDoubleClickTimer();
  void OnStopToReloadTimer();

  base::OneShotTimer<ReloadButton> double_click_timer_;
  base::OneShotTimer<ReloadButton> stop_to_reload_timer_;

  // These may be NULL when testing.
  LocationBarView* location_bar_;
  Browser* browser_;

  // The mode we should be in assuming no timers are running.
  Mode intended_mode_;

  // The currently-visible mode - this may differ from the intended mode.
  Mode visible_mode_;

  // The delay times for the timers.  These are members so that tests can modify
  // them.
  base::TimeDelta double_click_timer_delay_;
  base::TimeDelta stop_to_reload_timer_delay_;

  // TESTING ONLY
  // True if we should pretend the button is hovered.
  bool testing_mouse_hovered_;
  // Increments when we would tell the browser to "reload", so
  // test code can tell whether we did so (as there may be no |browser_|).
  int testing_reload_count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReloadButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELOAD_BUTTON_H__
