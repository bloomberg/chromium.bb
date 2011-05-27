// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_OOBE_DISPLAY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_OOBE_DISPLAY_H_
#pragma once

#include <string>
#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "ui/gfx/rect.h"

namespace views {
class View;
class Widget;
}

namespace chromeos {

class ScreenObserver;

class ViewsOobeDisplay : public OobeDisplay,
                         public ViewScreenDelegate {
 public:
  explicit ViewsOobeDisplay(const gfx::Rect& screen_bounds);
  virtual ~ViewsOobeDisplay();

  // OobeDisplay implementation:
  virtual void ShowScreen(WizardScreen* screen);
  virtual void HideScreen(WizardScreen* screen);
  virtual UpdateScreenActor* CreateUpdateScreenActor();
  virtual NetworkScreenActor* CreateNetworkScreenActor();
  virtual EulaScreenActor* CreateEulaScreenActor();
  virtual ViewScreenDelegate* CreateEnterpriseEnrollmentScreenActor();
  virtual ViewScreenDelegate* CreateUserImageScreenActor();
  virtual ViewScreenDelegate* CreateRegistrationScreenActor();
  virtual ViewScreenDelegate* CreateHTMLPageScreenActor();

  // Overridden from ViewScreenDelegate:
  virtual views::View* GetWizardView();
  virtual chromeos::ScreenObserver* GetObserver();
  virtual void SetScreenSize(const gfx::Size &screen_size);

  void SetScreenObserver(ScreenObserver* screen_observer);

 private:
  // Creates wizard screen window with the specified |bounds|.
  // If |initial_show| initial animation (window & background) is shown.
  // Otherwise only window is animated.
  views::Widget* CreateScreenWindow(const gfx::Rect& bounds,
                                    bool initial_show);

  // Returns bounds for the wizard screen host window in screen coordinates.
  // Calculates bounds using screen_bounds_.
  gfx::Rect GetWizardScreenBounds(int screen_width, int screen_height) const;

  // Widget we're showing in.
  views::Widget* widget_;

  // Contents view.
  views::View* contents_;

  // Used to calculate position of the wizard screen.
  gfx::Rect screen_bounds_;

  // Keeps current screen size.
  gfx::Size screen_size_;

  // Holds whether this is initial show.
  bool initial_show_;

  ScreenObserver* screen_observer_;

  FRIEND_TEST_ALL_PREFIXES(WizardControllerFlowTest, Accelerators);

  DISALLOW_COPY_AND_ASSIGN(ViewsOobeDisplay);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEWS_OOBE_DISPLAY_H_
