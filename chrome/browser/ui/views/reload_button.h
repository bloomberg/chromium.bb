// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELOAD_BUTTON_H__
#define CHROME_BROWSER_UI_VIEWS_RELOAD_BUTTON_H__

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/timer/timer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button_dropdown.h"

class CommandUpdater;
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

class ReloadButton : public views::ButtonDropDown,
                     public views::ButtonListener,
                     public ui::SimpleMenuModel::Delegate {
 public:
  enum Mode { MODE_RELOAD = 0, MODE_STOP };

  // The button's class name.
  static const char kViewClassName[];

  ReloadButton(LocationBarView* location_bar,
               CommandUpdater* command_updater);
  virtual ~ReloadButton();

  // Ask for a specified button state.  If |force| is true this will be applied
  // immediately.
  void ChangeMode(Mode mode, bool force);

  // Enable reload drop-down menu.
  void set_menu_enabled(bool enable) { menu_enabled_ = enable; }

  void LoadImages(ui::ThemeProvider* tp);

  // Overridden from views::View:
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p,
                              string16* tooltip) const OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from views::ButtonDropDown:
  virtual bool ShouldShowMenu() OVERRIDE;
  virtual void ShowDropDownMenu(ui::MenuSourceType source_type) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* /* button */,
                             const ui::Event& event) OVERRIDE;

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool IsCommandIdVisible(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  friend class ReloadButtonTest;

  ui::SimpleMenuModel* CreateMenuModel();

  void ExecuteBrowserCommand(int command, int event_flags);
  void ChangeModeInternal(Mode mode);

  void OnDoubleClickTimer();
  void OnStopToReloadTimer();

  base::OneShotTimer<ReloadButton> double_click_timer_;
  base::OneShotTimer<ReloadButton> stop_to_reload_timer_;

  // These may be NULL when testing.
  LocationBarView* location_bar_;
  CommandUpdater* command_updater_;

  // The mode we should be in assuming no timers are running.
  Mode intended_mode_;

  // The currently-visible mode - this may differ from the intended mode.
  Mode visible_mode_;

  // The delay times for the timers.  These are members so that tests can modify
  // them.
  base::TimeDelta double_click_timer_delay_;
  base::TimeDelta stop_to_reload_timer_delay_;

  // Indicates if reload menu is enabled.
  bool menu_enabled_;

  // The parent class's images_ member is used for the current images,
  // and this array is used to hold the alternative images.
  // We swap between the two when changing mode.
  gfx::ImageSkia alternate_images_[STATE_COUNT];

  // TESTING ONLY
  // True if we should pretend the button is hovered.
  bool testing_mouse_hovered_;
  // Increments when we would tell the browser to "reload", so
  // test code can tell whether we did so (as there may be no |browser_|).
  int testing_reload_count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReloadButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELOAD_BUTTON_H__
