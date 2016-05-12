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
  void AddLowPowerModeSuppression();
  void RemoveLowPowerModeSuppression();

  // ui::FullscreenLowPowerCoordinator implementation.
  void SetLowPowerLayerValid(bool valid) override;
  void WillLoseAcceleratedWidget() override;

 private:
  // Determine if we should be fullscreen low power mode, and enter or exit
  // the mode as needed.
  void EnterOrExitLowPowerModeIfNeeded();

  // The main fullscreen window.
  base::scoped_nsobject<NSWindow> content_window_ = nil;

  // Weak, reset by WillLoseAcceleratedWidget before it goes away.
  ui::AcceleratedWidgetMac* widget_ = nullptr;

  // The window that we will put in front of the main fullscreen window when we
  // can.
  base::scoped_nsobject<FullscreenLowPowerWindow> low_power_window_;

  // Set by the AcceleratedWidgetHost with each frame. This must be true to
  // enter low power mode.
  bool low_power_layer_valid_ = false;

  // The balance of calls to Add/RemoveLowPowerModeSuppression. This must be
  // zero to enter low power mode.
  int suppression_count_ = 0;

  // Updated by EnterOrExitLowPowerModeIfNeeded.
  bool in_low_power_mode_ = false;
};

#endif  // CHROME_BROWSER_UI_COCOA_FULLSCREEN_LOW_POWER_COORDINATOR_H_
