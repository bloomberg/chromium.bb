// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/webui/wrench_menu_ui.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chromeos/views/native_menu_webui.h"
#include "chrome/browser/chromeos/views/webui_menu_widget.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/menu/menu_2.h"

namespace {

class WrenchMenuSourceDelegate : public chromeos::MenuSourceDelegate {
 public:
  virtual void AddCustomConfigValues(DictionaryValue* config) const {
    // Resources that are necessary to build wrench menu.
    config->SetInteger("IDC_CUT", IDC_CUT);
    config->SetInteger("IDC_COPY", IDC_COPY);
    config->SetInteger("IDC_PASTE", IDC_PASTE);
    config->SetInteger("IDC_ZOOM_MINUS", IDC_ZOOM_MINUS);
    config->SetInteger("IDC_ZOOM_PLUS", IDC_ZOOM_PLUS);
    config->SetInteger("IDC_FULLSCREEN", IDC_FULLSCREEN);

    config->SetString("IDS_EDIT2", l10n_util::GetStringUTF8(IDS_EDIT2));
    config->SetString("IDS_ZOOM_MENU2",
                      l10n_util::GetStringUTF8(IDS_ZOOM_MENU2));
    config->SetString("IDS_CUT", l10n_util::GetStringUTF8(IDS_CUT));
    config->SetString("IDS_COPY", l10n_util::GetStringUTF8(IDS_COPY));
    config->SetString("IDS_PASTE", l10n_util::GetStringUTF8(IDS_PASTE));
  }
};

}  // namespace

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
            CreateMenuUIHTMLSource(new WrenchMenuSourceDelegate(),
                                   chrome::kChromeUIWrenchMenu,
                                   "WrenchMenu"  /* class name */,
                                   IDR_WRENCH_MENU_JS,
                                   IDR_WRENCH_MENU_CSS))) {
  registrar_.Add(this, NotificationType::ZOOM_LEVEL_CHANGED,
                 Source<Profile>(GetProfile()));
}

void WrenchMenuUI::ModelUpdated(const ui::MenuModel* new_model) {
  MenuUI::ModelUpdated(new_model);
  UpdateZoomControls();
}

void WrenchMenuUI::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  DCHECK_EQ(NotificationType::ZOOM_LEVEL_CHANGED, type.value);
  UpdateZoomControls();
}

void WrenchMenuUI::UpdateZoomControls() {
  WebUIMenuWidget* widget = WebUIMenuWidget::FindWebUIMenuWidget(
      tab_contents()->GetNativeView());
  if (!widget || !widget->is_root())
    return;
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;
  TabContents* selected_tab = browser->GetSelectedTabContents();
  bool enable_increment = false;
  bool enable_decrement = false;
  int zoom = 100;
  if (selected_tab)
    zoom = selected_tab->GetZoomPercent(&enable_increment, &enable_decrement);

  DictionaryValue params;
  params.SetBoolean("plus", enable_increment);
  params.SetBoolean("minus", enable_decrement);
  params.SetString("percent", l10n_util::GetStringFUTF16(
      IDS_ZOOM_PERCENT, UTF8ToUTF16(base::IntToString(zoom))));
  CallJavascriptFunction(L"updateZoomControls", params);
}

views::Menu2* WrenchMenuUI::CreateMenu2(ui::MenuModel* model) {
  views::Menu2* menu = new views::Menu2(model);
  NativeMenuWebUI::SetMenuURL(
      menu, GURL(StringPrintf("chrome://%s", chrome::kChromeUIWrenchMenu)));
  return menu;
}

}  // namespace chromeos
