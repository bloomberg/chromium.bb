// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FULLSCREEN_LOW_POWER_COORDINATOR_H_
#define CHROME_BROWSER_UI_COCOA_FULLSCREEN_LOW_POWER_COORDINATOR_H_

#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/accelerated_widget_mac/fullscreen_low_power_coordinator.h"

@class FullscreenLowPowerWindow;

// This class coordinates switching to and from the fullscreen low power
// NSWindow. This is created by the BrowserWindowController when entering
// fullscreen mode. If it is ``safe'' to use the fullscreen low power window,
// this will move that NSWindow in front of the browser NSWindow.
//
// Whether or not it is ``safe'' to use the fullscreen low power window depends
// on two sources of input: The AcceleratedWidgetMac, which tells this whether
// or not the current web content can be displayed in fullscreen low power mode,
// and the BrowserWindowController, which tells this whether or not there is any
// UI that the fullscreen low power window would cover.
class FullscreenLowPowerCoordinatorCocoa
    : public ui::FullscreenLowPowerCoordinator {
 public:
  FullscreenLowPowerCoordinatorCocoa(NSWindow* content_window,
                                     ui::AcceleratedWidgetMac* widget);
  ~FullscreenLowPowerCoordinatorCocoa();

  // Interface to BrowserWindowController.
  NSWindow* GetFullscreenLowPowerWindow();
  // Note that the initial transition state is in-transition.
  void SetInFullscreenTransition(bool in_fullscreen_transition);
  void SetLayoutParameters(const NSRect& toolbar_frame,
                           const NSRect& infobar_frame,
                           const NSRect& content_frame,
                           const NSRect& download_shelf_frame);
  void SetHasActiveSheet(bool has_sheet);
  void ChildWindowsChanged();

  // ui::FullscreenLowPowerCoordinator implementation.
  void SetLowPowerLayerValid(bool valid) override;
  void WillLoseAcceleratedWidget() override;

  // Call EnterOrExitLowPowerModeIfNeeded to wind down counters.
  void TickEnterOrExitForTesting();

 private:
  // Determine if we should be fullscreen low power mode, and enter or exit
  // the mode as needed.
  void EnterOrExitLowPowerModeIfNeeded();

  // The main fullscreen window.
  base::scoped_nsobject<NSWindow> content_window_;

  // Weak, reset by WillLoseAcceleratedWidget before it goes away.
  ui::AcceleratedWidgetMac* widget_ = nullptr;

  // The window that we will put in front of the main fullscreen window when we
  // can.
  base::scoped_nsobject<FullscreenLowPowerWindow> low_power_window_;

  // Don't use the fullscreen low power window until we have completely
  // transitioned to low power mode.
  bool allowed_by_fullscreen_transition_ = false;

  // Set by the AcceleratedWidgetHost with each frame to indicate if the low
  // power layer's contents are valid.
  bool low_power_layer_valid_ = false;

  // Set if the NSView hierarchy allows low power mode. Low power mode is only
  // allowed when nothing but the web contents is on-screen.
  bool allowed_by_nsview_layout_ = true;

  // Set if there are no NSWindows that would be covered by the fullscreen low
  // power window.
  bool allowed_by_child_windows_ = false;

  // Set if there are no sheets (modal dialogues) that would be covered by the
  // fullscreen low power window.
  bool allowed_by_active_sheet_ = false;

  // Updated by EnterOrExitLowPowerModeIfNeeded.
  enum State {
    // The fullscreen low power window is hidden behind the main window.
    Disabled,
    // The fullscreen low power window is still hidden, but is counting up
    // transition frames before showing itself.
    WarmingUp,
    // The fullscreen low power window is in front of the main window.
    Enabled,
  };
  State state_ = Disabled;
  uint64_t frames_in_state_ = 0;
};

#endif  // CHROME_BROWSER_UI_COCOA_FULLSCREEN_LOW_POWER_COORDINATOR_H_
