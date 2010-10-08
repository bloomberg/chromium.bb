// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_WRENCH_MENU_UI_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_WRENCH_MENU_UI_H_
#pragma once

#include "chrome/browser/chromeos/dom_ui/menu_ui.h"

namespace chromeos {

class WrenchMenuUI : public MenuUI {
 public:
  explicit WrenchMenuUI(TabContents* contents);

  // MenuUI overrides:
  virtual void AddCustomConfigValues(DictionaryValue* config) const;

  // Create HTML Data source for the menu.  Extended menu
  // implementation may provide its own menu implmentation.
  virtual ChromeURLDataManager::DataSource* CreateDataSource();

 private:
  DISALLOW_COPY_AND_ASSIGN(WrenchMenuUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_WRENCH_MENU_UI_H_
