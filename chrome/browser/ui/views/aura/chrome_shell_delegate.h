// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_CHROME_SHELL_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_CHROME_SHELL_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura_shell/launcher/launcher_types.h"
#include "ui/aura_shell/shell_delegate.h"

class Browser;
class StatusAreaHostAura;

namespace views {
class View;
}

class ChromeShellDelegate : public aura_shell::ShellDelegate {
 public:
  ChromeShellDelegate();
  virtual ~ChromeShellDelegate();

  static ChromeShellDelegate* instance() { return instance_; }

  views::View* GetStatusAreaForTest();

  // Returns whether a launcher item should be created for |browser|. If an item
  // should be created |type| is set to the launcher type to create.
  static bool ShouldCreateLauncherItemForBrowser(
      Browser* browser,
      aura_shell::LauncherItemType* type);

  // aura_shell::ShellDelegate overrides;
  virtual void CreateNewWindow() OVERRIDE;
  virtual views::Widget* CreateStatusArea() OVERRIDE;
  virtual void ShowApps() OVERRIDE;
  virtual void LauncherItemClicked(
      const aura_shell::LauncherItem& item) OVERRIDE;
  virtual bool ConfigureLauncherItem(aura_shell::LauncherItem* item) OVERRIDE;

 private:
  static ChromeShellDelegate* instance_;

  scoped_ptr<StatusAreaHostAura> status_area_host_;

  DISALLOW_COPY_AND_ASSIGN(ChromeShellDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_CHROME_SHELL_DELEGATE_H_
