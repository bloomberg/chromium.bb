// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_H_

#include "ash/new_window_delegate.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

class Browser;

class ChromeNewWindowDelegate : public ash::NewWindowDelegate {
 public:
  ChromeNewWindowDelegate();
  virtual ~ChromeNewWindowDelegate();

  // Overridden from ash::NewWindowDelegate:
  virtual void NewTab() OVERRIDE;
  virtual void NewWindow(bool incognito) OVERRIDE;
  virtual void RestoreTab() OVERRIDE;
  virtual void ShowTaskManager() OVERRIDE;
  virtual void OpenFeedbackPage() OVERRIDE;

 private:
  class TabRestoreHelper;

  scoped_ptr<TabRestoreHelper> tab_restore_helper_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNewWindowDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_NEW_WINDOW_DELEGATE_H_
