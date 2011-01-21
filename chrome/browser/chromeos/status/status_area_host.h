// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_HOST_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_HOST_H_
#pragma once

#include "gfx/native_widget_types.h"

namespace views {
class View;
}  // namespace views

class Profile;

namespace chromeos {

// This class is an abstraction decoupling StatusAreaView from its host
// window.
class StatusAreaHost {
 public:
  // Returns the Profile if this status area is inside the browser and has a
  // profile. Otherwise, returns NULL.
  virtual Profile* GetProfile() const = 0;

  // Returns native window hosting the status area.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Indicates if options dialog related to the button specified should be
  // shown.
  virtual bool ShouldOpenButtonOptions(
      const views::View* button_view) const = 0;

  // Opens options dialog related to the button specified.
  virtual void OpenButtonOptions(const views::View* button_view) = 0;

  // Executes browser command.
  virtual void ExecuteBrowserCommand(int id) const = 0;

  // The type of screen the host window is on.
  enum ScreenMode {
    kLoginMode,  // The host is for the OOBE/login screens.
    kBrowserMode,  // The host is for browser.
    kScreenLockerMode,  // The host is for screen locker.
  };

  // Returns the type of screen.
  virtual ScreenMode GetScreenMode() const = 0;

 protected:
  virtual ~StatusAreaHost() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_HOST_H_
