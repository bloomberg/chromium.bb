// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_browser_main_linux.h"

namespace chromeos {
class BrightnessObserver;
class SessionManagerObserver;

#if defined(USE_AURA)
class InitialBrowserWindowObserver;
#endif
}  // namespace chromeos

class ChromeBrowserMainPartsChromeos : public ChromeBrowserMainPartsLinux {
 public:
  explicit ChromeBrowserMainPartsChromeos(
      const content::MainFunctionParams& parameters);
  virtual ~ChromeBrowserMainPartsChromeos();

  // content::BrowserMainParts overrides.
  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;

 private:
  scoped_ptr<chromeos::BrightnessObserver> brightness_observer_;
  scoped_ptr<chromeos::SessionManagerObserver> session_manager_observer_;

#if defined(USE_AURA)
  scoped_ptr<chromeos::InitialBrowserWindowObserver>
      initial_browser_window_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsChromeos);
};

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
