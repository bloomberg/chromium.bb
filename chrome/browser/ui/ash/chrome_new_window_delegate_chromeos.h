// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_CHROMEOS_H_

#include "chrome/browser/ui/ash/chrome_new_window_delegate.h"

class ChromeNewWindowDelegateChromeos : public ChromeNewWindowDelegate {
 public:
  ChromeNewWindowDelegateChromeos();
  ~ChromeNewWindowDelegateChromeos() override;

  // Overridden from ash::NewWindowDelegate:
  void OpenFileManager() override;
  void OpenCrosh() override;
  void OpenGetHelp() override;
  void ShowKeyboardOverlay() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeNewWindowDelegateChromeos);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_CHROMEOS_H_
