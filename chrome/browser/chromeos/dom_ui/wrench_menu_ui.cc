// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/wrench_menu_ui.h"

#include "base/values.h"
#include "chrome/app/chrome_dll_resource.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
//
// WrenchMenuUI
//
////////////////////////////////////////////////////////////////////////////////

WrenchMenuUI::WrenchMenuUI(TabContents* contents) : chromeos::MenuUI(contents) {
}

void WrenchMenuUI::AddCustomConfigValues(DictionaryValue* config) const {
  // These command ids are to create customized menu items for wrench menu.
  config->SetInteger("IDC_CUT", IDC_CUT);
  config->SetInteger("IDC_ZOOM_MINUS", IDC_ZOOM_MINUS);
}

ChromeURLDataManager::DataSource* WrenchMenuUI::CreateDataSource() {
  return CreateMenuUIHTMLSource(*this, GetProfile(),
                                "WrenchMenu", "wrench_menu.js");
}

}  // namespace chromeos
