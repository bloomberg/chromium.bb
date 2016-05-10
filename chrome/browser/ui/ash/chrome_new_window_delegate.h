// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_H_

#include <memory>

#include "ash/new_window_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

class Browser;

class ChromeNewWindowDelegate : public ash::NewWindowDelegate {
 public:
  ChromeNewWindowDelegate();
  ~ChromeNewWindowDelegate() override;

  // Overridden from ash::NewWindowDelegate:
  void NewTab() override;
  void NewWindow(bool incognito) override;
  void OpenFileManager() override;
  void OpenCrosh() override;
  void OpenGetHelp() override;
  void RestoreTab() override;
  void ShowKeyboardOverlay() override;
  void ShowTaskManager() override;
  void OpenFeedbackPage() override;

 private:
  class TabRestoreHelper;

  std::unique_ptr<TabRestoreHelper> tab_restore_helper_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNewWindowDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_H_
