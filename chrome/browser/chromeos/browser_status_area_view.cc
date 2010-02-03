// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/browser_status_area_view.h"

#include "app/l10n_util.h"
#include "app/theme_provider.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_menu_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/chromeos/chromeos_browser_view.h"
#include "chrome/browser/chromeos/status_area_button.h"
#include "chrome/browser/chromeos/status_area_view.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/theme_background.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/menu/menu.h"
#include "views/controls/menu/menu_2.h"
#include "views/window/window.h"

namespace chromeos {

class OptionsMenuModel : public menus::SimpleMenuModel,
                         public menus::SimpleMenuModel::Delegate {
 public:
  // These extra command IDs must be unique when combined with the options,
  // so we just pick up the numbering where that stops.
  enum OtherCommands {
    CREATE_NEW_WINDOW = StatusAreaView::OPEN_TABS_ON_RIGHT + 1,
  };

  explicit OptionsMenuModel(Browser* browser)
      : SimpleMenuModel(this),
        browser_(browser) {
    AddItemWithStringId(IDC_COMPACT_NAVBAR, IDS_COMPACT_NAVBAR);
    AddSeparator();

    AddItem(static_cast<int>(StatusAreaView::OPEN_TABS_ON_LEFT),
            ASCIIToUTF16("Open tabs on left"));
    AddItem(static_cast<int>(StatusAreaView::OPEN_TABS_CLOBBER),
            ASCIIToUTF16("Open tabs clobber"));
    AddItem(static_cast<int>(StatusAreaView::OPEN_TABS_ON_RIGHT),
            ASCIIToUTF16("Open tabs on right"));
  }
  virtual ~OptionsMenuModel() {
  }

  // SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const {
    return StatusAreaView::GetOpenTabsMode() == command_id;
  }
  virtual bool IsCommandIdEnabled(int command_id) const {
    return true;
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      menus::Accelerator* accelerator) {
    return false;
  }
  virtual void ExecuteCommand(int command_id) {
    switch (command_id) {
      case IDC_COMPACT_NAVBAR:
        browser_->ExecuteCommand(command_id);
        break;
      case StatusAreaView::OPEN_TABS_ON_LEFT:
      case StatusAreaView::OPEN_TABS_CLOBBER:
      case StatusAreaView::OPEN_TABS_ON_RIGHT:
        StatusAreaView::SetOpenTabsMode(
            static_cast<StatusAreaView::OpenTabsMode>(command_id));
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(OptionsMenuModel);
};

BrowserStatusAreaView::BrowserStatusAreaView(ChromeosBrowserView* browser_view)
    : StatusAreaView(browser_view),
      browser_view_(browser_view),
      menu_view_(NULL) {
}

void BrowserStatusAreaView::Init() {
  StatusAreaView::Init();
  ThemeProvider* theme = browser_view_->frame()->GetThemeProviderForFrame();

// Menu.
  menu_view_ = new StatusAreaButton(this);
  menu_view_->SetIcon(*theme->GetBitmapNamed(IDR_STATUSBAR_MENU));
  AddChildView(menu_view_);

  set_background(new ThemeBackground(browser_view_));

  app_menu_contents_.reset(CreateAppMenuModel(this));
  app_menu_menu_.reset(new views::Menu2(app_menu_contents_.get()));
}

AppMenuModel* BrowserStatusAreaView::CreateAppMenuModel(
    menus::SimpleMenuModel::Delegate* delegate) {
  Browser* browser = browser_view_->browser();
  AppMenuModel* menu_model = new AppMenuModel(delegate, browser);

  // Options menu always uses StatusAreaView as delegate, so
  // we can reuse it.
  if (!options_menu_contents_.get())
    options_menu_contents_.reset(new OptionsMenuModel(browser));

  int sync_index = menu_model->GetIndexOfCommandId(IDC_SYNC_BOOKMARKS);
  DCHECK_GE(sync_index, 0);
  menu_model->InsertItemWithStringIdAt(
      sync_index + 1, IDC_CLEAR_BROWSING_DATA, IDS_CLEAR_BROWSING_DATA);
  menu_model->InsertSeparatorAt(sync_index + 1);

  int options_index = menu_model->GetIndexOfCommandId(IDC_OPTIONS);
  DCHECK_GE(options_index, 0);
  menu_model->InsertSubMenuAt(
      options_index + 1,
      ASCIIToUTF16("Compact nav bar"), options_menu_contents_.get());
  return menu_model;
}

bool BrowserStatusAreaView::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_SHOW_BOOKMARK_BAR)
    return browser_view_->browser()->profile()->GetPrefs()->GetBoolean(
        prefs::kShowBookmarkBar);
  return false;
}

bool BrowserStatusAreaView::IsCommandIdEnabled(int command_id) const {
  Browser* browser = browser_view_->browser();
  if (command_id == IDC_RESTORE_TAB)
    return browser->CanRestoreTab();
  return browser->command_updater()->IsCommandEnabled(command_id);
}

bool BrowserStatusAreaView::GetAcceleratorForCommandId(
    int command_id,
    menus::Accelerator* accelerator) {
  return false;
}

void BrowserStatusAreaView::ExecuteCommand(int command_id) {
  browser_view_->browser()->ExecuteCommand(command_id);
}

void BrowserStatusAreaView::RunMenu(views::View* source, const gfx::Point& pt) {
  app_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

}  // namespace chromeos
