// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status_area_view.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/theme_provider.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_menu_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/chromeos/clock_menu_button.h"
#include "chrome/browser/chromeos/language_menu_button.h"
#include "chrome/browser/chromeos/network_menu_button.h"
#include "chrome/browser/chromeos/power_menu_button.h"
#include "chrome/browser/chromeos/status_area_button.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/frame/browser_view.h"
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

// Number of pixels to pad on the left border.
const int kLeftBorder = 1;
// Number of pixels to separate the clock from the next item on the right.
const int kClockSeparation = 4;
// Number of pixels to separate the language selector from the next item
// on the right.
const int kLanguageSeparation = 4;

// BrowserWindowGtk tiles its image with this offset
const int kCustomFrameBackgroundVerticalOffset = 15;

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

// Default to opening new tabs on the left.
StatusAreaView::OpenTabsMode StatusAreaView::open_tabs_mode_ =
    StatusAreaView::OPEN_TABS_ON_LEFT;

StatusAreaView::StatusAreaView(BrowserView* browser_view)
    : browser_view_(browser_view),
      clock_view_(NULL),
      language_view_(NULL),
      network_view_(NULL),
      battery_view_(NULL),
      menu_view_(NULL) {
}

void StatusAreaView::Init() {
  ThemeProvider* theme = browser_view_->frame()->GetThemeProviderForFrame();
  Browser* browser = browser_view_->browser();
  // Language.
  language_view_ = new LanguageMenuButton(browser);
  AddChildView(language_view_);

  // Clock.
  clock_view_ = new ClockMenuButton(browser);
  AddChildView(clock_view_);

  // Battery.
  battery_view_ = new PowerMenuButton();
  AddChildView(battery_view_);

  // Network.
  network_view_ = new NetworkMenuButton(
      browser_view_->GetWindow()->GetNativeWindow());

  AddChildView(network_view_);

  // Menu.
  menu_view_ = new StatusAreaButton(this);
  menu_view_->SetIcon(*theme->GetBitmapNamed(IDR_STATUSBAR_MENU));
  AddChildView(menu_view_);

  set_background(new ThemeBackground(browser_view_));

  app_menu_contents_.reset(CreateAppMenuModel(this));
  app_menu_menu_.reset(new views::Menu2(app_menu_contents_.get()));
}

void StatusAreaView::Update() {
  menu_view_->SetVisible(!browser_view_->IsToolbarVisible());
}

gfx::Size StatusAreaView::GetPreferredSize() {
  // Start with padding.
  int result_w = kLeftBorder + kClockSeparation + kLanguageSeparation;
  int result_h = 0;
  for (int i = 0; i < GetChildViewCount(); i++) {
    views::View* cur = GetChildViewAt(i);
    if (cur->IsVisible()) {
      gfx::Size cur_size = cur->GetPreferredSize();
      // Add each width.
      result_w += cur_size.width();
      // Use max height.
      result_h = std::max(result_h, cur_size.height());
    }
  }
  return gfx::Size(result_w, result_h);
}

void StatusAreaView::Layout() {
  int cur_x = kLeftBorder;
  for (int i = 0; i < GetChildViewCount(); i++) {
    views::View* cur = GetChildViewAt(i);
    if (cur->IsVisible()) {
      gfx::Size cur_size = cur->GetPreferredSize();
      int cur_y = (height() - cur_size.height()) / 2;

      // Handle odd number of pixels.
      cur_y += (height() - cur_size.height()) % 2;

      // Put next in row horizontally, and center vertically.
      cur->SetBounds(cur_x, cur_y, cur_size.width(), cur_size.height());

      cur_x += cur_size.width();

      // Buttons have built in padding, but clock and language status don't.
      if (cur == clock_view_)
        cur_x += kClockSeparation;
      else if (cur == language_view_)
        cur_x += kLanguageSeparation;
    }
  }
}

// static
StatusAreaView::OpenTabsMode StatusAreaView::GetOpenTabsMode() {
  return open_tabs_mode_;
}

// static
void StatusAreaView::SetOpenTabsMode(OpenTabsMode mode) {
  open_tabs_mode_ = mode;
}

AppMenuModel* StatusAreaView::CreateAppMenuModel(
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

bool StatusAreaView::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_SHOW_BOOKMARK_BAR)
    return browser_view_->browser()->profile()->GetPrefs()->GetBoolean(
        prefs::kShowBookmarkBar);
  return false;
}

bool StatusAreaView::IsCommandIdEnabled(int command_id) const {
  Browser* browser = browser_view_->browser();
  if (command_id == IDC_RESTORE_TAB)
    return browser->CanRestoreTab();
  return browser->command_updater()->IsCommandEnabled(command_id);
}

bool StatusAreaView::GetAcceleratorForCommandId(
    int command_id,
    menus::Accelerator* accelerator) {
  return false;
}

void StatusAreaView::ExecuteCommand(int command_id) {
  browser_view_->browser()->ExecuteCommand(command_id);
}

void StatusAreaView::RunMenu(views::View* source, const gfx::Point& pt) {
  app_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

}  // namespace chromeos
