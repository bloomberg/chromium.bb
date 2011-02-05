// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_SCREEN_H_
#pragma once

#include "ui/gfx/canvas.h"

class WizardScreen;
namespace chromeos {
class ScreenObserver;
}  // namespace chromeos
namespace gfx {
class Size;
}  // namespace gfx
namespace views {
class View;
}  // namespace views

// Interface that login wizard exposes to its screens.
class WizardScreenDelegate {
 public:
  // Returns top level view of the wizard.
  virtual views::View* GetWizardView() = 0;

  // Returns observer screen should notify.
  virtual chromeos::ScreenObserver* GetObserver(WizardScreen* screen) = 0;

  // Forces the current screen to be shown immediately.
  virtual void ShowCurrentScreen() = 0;

 protected:
  virtual ~WizardScreenDelegate() {}
};

// Interface that defines login wizard screens.
// Also holds a reference to a delegate.
class WizardScreen {
 public:
  // Makes wizard screen visible.
  virtual void Show() = 0;
  // Makes wizard screen invisible.
  virtual void Hide() = 0;
  // Returns the size the screen.
  virtual gfx::Size GetScreenSize() const = 0;

 protected:
  explicit WizardScreen(WizardScreenDelegate* delegate): delegate_(delegate) {}
  virtual ~WizardScreen() {}

  WizardScreenDelegate* delegate() { return delegate_; }

  // Refreshes screen state. Should be called after view is made visible.
  virtual void Refresh() = 0;

 private:
  WizardScreenDelegate* delegate_;
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_SCREEN_H_
