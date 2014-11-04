// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CHANGE_OBSERVER_CHROMEOS_H_
#define ASH_DISPLAY_DISPLAY_CHANGE_OBSERVER_CHROMEOS_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {

class DisplayInfo;
struct DisplayMode;

// An object that observes changes in display configuration and
// update DisplayManagers.
class DisplayChangeObserver : public ui::DisplayConfigurator::StateController,
                              public ui::DisplayConfigurator::Observer,
                              public ui::InputDeviceEventObserver,
                              public ShellObserver {
 public:
  // Returns the mode list for internal display.
  ASH_EXPORT static std::vector<DisplayMode> GetInternalDisplayModeList(
      const DisplayInfo& display_info,
      const ui::DisplayConfigurator::DisplayState& output);

  // Returns the resolution list.
  ASH_EXPORT static std::vector<DisplayMode> GetExternalDisplayModeList(
      const ui::DisplayConfigurator::DisplayState& output);

  DisplayChangeObserver();
  virtual ~DisplayChangeObserver();

  // ui::DisplayConfigurator::StateController overrides:
  virtual ui::MultipleDisplayState GetStateForDisplayIds(
      const std::vector<int64>& outputs) const override;
  virtual bool GetResolutionForDisplayId(int64 display_id,
                                         gfx::Size* size) const override;

  // Overriden from ui::DisplayConfigurator::Observer:
  virtual void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& outputs) override;

  // Overriden from ui::InputDeviceEventObserver:
  virtual void OnTouchscreenDeviceConfigurationChanged() override;
  virtual void OnKeyboardDeviceConfigurationChanged() override;

  // Overriden from ShellObserver:
  virtual void OnAppTerminating() override;

  // Exposed for testing.
  ASH_EXPORT static float FindDeviceScaleFactor(float dpi);

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayChangeObserver);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CHANGE_OBSERVER_CHROMEOS_H_
