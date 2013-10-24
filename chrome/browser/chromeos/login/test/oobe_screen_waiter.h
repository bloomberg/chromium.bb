// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREEN_WAITER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREEN_WAITER_H_

#include "base/basictypes.h"
#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace content {
class MessageLoopRunner;
}

namespace chromeos {

// A waiter that blocks until the expected oobe screen is reached.
class OobeScreenWaiter : public OobeUI::Observer {
 public:
  explicit OobeScreenWaiter(OobeDisplay::Screen expected_screen);
  virtual ~OobeScreenWaiter();

  void Wait();

  // OobeUI::Observer implementation:
  virtual void OnCurrentScreenChanged(
        OobeDisplay::Screen current_screen,
        OobeDisplay::Screen new_screen) OVERRIDE;

 private:
  OobeUI* GetOobeUI();

  bool waiting_for_screen_;
  OobeDisplay::Screen expected_screen_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(OobeScreenWaiter);
};


}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREEN_WAITER_H_
