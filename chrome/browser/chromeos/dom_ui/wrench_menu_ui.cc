// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/wrench_menu_ui.h"

#include "base/values.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/chromeos/views/native_menu_domui.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "views/controls/menu/menu_2.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
//
// WrenchMenuUI
//
////////////////////////////////////////////////////////////////////////////////

WrenchMenuUI::WrenchMenuUI(TabContents* contents)
    : chromeos::MenuUI(
        contents,
        ALLOW_THIS_IN_INITIALIZER_LIST(
            CreateMenuUIHTMLSource(*this,
                                   chrome::kChromeUIWrenchMenu,
                                   "Menu",
                                   IDR_WRENCH_MENU_JS))) {
}

void WrenchMenuUI::AddCustomConfigValues(DictionaryValue* config) const {
  // These command ids are to create customized menu items for wrench menu.
  config->SetInteger("IDC_CUT", IDC_CUT);
  config->SetInteger("IDC_ZOOM_MINUS", IDC_ZOOM_MINUS);
}

views::Menu2* WrenchMenuUI::CreateMenu2(menus::MenuModel* model) {
  views::Menu2* menu = new views::Menu2(model);
  NativeMenuDOMUI::SetMenuURL(
      menu, GURL(StringPrintf("chrome://%s", chrome::kChromeUIWrenchMenu)));
  return menu;
}

}  // namespace chromeos
