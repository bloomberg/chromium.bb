// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_CHROME_LAUNCHER_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_CHROME_LAUNCHER_DELEGATE_H_
#pragma once

#include "ash/launcher/launcher_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

class ChromeLauncherDelegate : public ash::LauncherDelegate {
 public:
  ChromeLauncherDelegate();
  virtual ~ChromeLauncherDelegate();

  // ash::LauncherDelegate overrides:
  virtual void CreateNewWindow() OVERRIDE;
  virtual void ItemClicked(const ash::LauncherItem& item) OVERRIDE;
  virtual int GetBrowserShortcutResourceId() OVERRIDE;
  virtual string16 GetTitle(const ash::LauncherItem& item) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_LAUNCHER_CHROME_LAUNCHER_DELEGATE_H_
