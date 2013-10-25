// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_CHROMEOS_H_

#include "chrome/browser/ui/ash/chrome_new_window_delegate.h"

class ChromeNewWindowDelegateChromeos : public ChromeNewWindowDelegate {
 public:
  ChromeNewWindowDelegateChromeos();
  virtual ~ChromeNewWindowDelegateChromeos();

  // Overridden from ash::NewWindowDelegate:
  virtual void OpenFileManager() OVERRIDE;
  virtual void OpenCrosh() OVERRIDE;
  virtual void ShowKeyboardOverlay() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeNewWindowDelegateChromeos);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_CHROMEOS_H_
