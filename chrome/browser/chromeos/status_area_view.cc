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
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/clock_menu_button.h"
#include "chrome/browser/chromeos/network_menu_button.h"
#include "chrome/browser/chromeos/power_menu_button.h"
#include "chrome/browser/chromeos/status_area_button.h"
#if !defined(TOOLKIT_VIEWS)
#include "chrome/browser/gtk/browser_window_gtk.h"
#endif
#include "chrome/browser/profile.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/menu/menu.h"
#include "views/controls/menu/simple_menu_model.h"

namespace {

// Number of pixels to pad on the left border.
const int kLeftBorder = 1;
// Number of pixels to separate the clock from the next item on the right.
const int kClockSeparation = 4;

// BrowserWindowGtk tiles its image with this offset
const int kCustomFrameBackgroundVerticalOffset = 15;

class OptionsMenuModel : public views::SimpleMenuModel,
                         public views::SimpleMenuModel::Delegate {
 public:
  // These extra command IDs must be unique when combined with the options,
  // so we just pick up the numbering where that stops.
  enum OtherCommands {
    CREATE_NEW_WINDOW = StatusAreaView::OPEN_TABS_ON_RIGHT + 1,
  };

  explicit OptionsMenuModel(Browser* browser,
                            views::SimpleMenuModel::Delegate* delegate)
      : SimpleMenuModel(this),
        browser_(browser) {
    AddItem(static_cast<int>(CREATE_NEW_WINDOW),
            ASCIIToUTF16("New window"));

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
      views::Accelerator* accelerator) {
    return false;
  }
  virtual void ExecuteCommand(int command_id) {
    switch (command_id) {
      case CREATE_NEW_WINDOW:
#if defined(TOOLKIT_VIEWS)
        // TODO(oshima): Implement accelerator to enable/disable
        // compact nav bar.
#else
        // Reach into the GTK browser window and enable the flag to create the
        // next window as a compact nav one.
        // TODO(brettw) this is an evil hack, and is here so this can be tested.
        // Remove it eventually.
        static_cast<BrowserWindowGtk*>(browser_->window())->
            set_next_window_should_use_compact_nav();
        browser_->ExecuteCommand(IDC_NEW_WINDOW);
#endif
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

}  // namespace

// Default to opening new tabs on the left.
StatusAreaView::OpenTabsMode StatusAreaView::open_tabs_mode_ =
    StatusAreaView::OPEN_TABS_ON_LEFT;

StatusAreaView::StatusAreaView(Browser* browser,
                               gfx::NativeWindow window)
    : browser_(browser),
      window_(window),
      clock_view_(NULL),
      network_view_(NULL),
      battery_view_(NULL),
      menu_view_(NULL) {
}

void StatusAreaView::Init() {
  ThemeProvider* theme = browser_->profile()->GetThemeProvider();

  // Clock.
  clock_view_ = new ClockMenuButton(browser_);
  AddChildView(clock_view_);

  // Battery.
  battery_view_ = new PowerMenuButton();
  AddChildView(battery_view_);

  // Network.
  network_view_ = new NetworkMenuButton(window_);
  AddChildView(network_view_);

  // Menu.
  menu_view_ = new StatusAreaButton(this);
  menu_view_->SetIcon(*theme->GetBitmapNamed(IDR_STATUSBAR_MENU));
  AddChildView(menu_view_);
}

gfx::Size StatusAreaView::GetPreferredSize() {
  // Start with padding.
  int result_w = kLeftBorder + kClockSeparation;
  int result_h = 0;
  for (int i = 0; i < GetChildViewCount(); i++) {
    gfx::Size cur_size = GetChildViewAt(i)->GetPreferredSize();
    // Add each width.
    result_w += cur_size.width();
    // Use max height.
    result_h = std::max(result_h, cur_size.height());
  }
  return gfx::Size(result_w, result_h);
}

void StatusAreaView::Layout() {
  int cur_x = kLeftBorder;
  for (int i = 0; i < GetChildViewCount(); i++) {
    views::View* cur = GetChildViewAt(i);
    gfx::Size cur_size = cur->GetPreferredSize();
    int cur_y = (height() - cur_size.height()) / 2;

    // Handle odd number of pixels.
    cur_y += (height() - cur_size.height()) % 2;

    // Put next in row horizontally, and center vertically.
    cur->SetBounds(cur_x, cur_y, cur_size.width(), cur_size.height());

    cur_x += cur_size.width();

    // Buttons have built in padding, but clock doesn't.
    if (cur == clock_view_)
      cur_x += kClockSeparation;
  }
}

void StatusAreaView::Paint(gfx::Canvas* canvas) {
  ThemeProvider* theme = browser_->profile()->GetThemeProvider();

  // Fill the background.
  int image_name;
  if (browser_->window()->IsActive()) {
    image_name = browser_->profile()->IsOffTheRecord() ?
                 IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME;
  } else {
    image_name = browser_->profile()->IsOffTheRecord() ?
                 IDR_THEME_FRAME_INCOGNITO_INACTIVE : IDR_THEME_FRAME_INACTIVE;
  }
  SkBitmap* background = theme->GetBitmapNamed(image_name);
  canvas->TileImageInt(
      *background,
      0, kCustomFrameBackgroundVerticalOffset,
      0, 0,
      width(), height());
}

// static
StatusAreaView::OpenTabsMode StatusAreaView::GetOpenTabsMode() {
  return open_tabs_mode_;
}

// static
void StatusAreaView::SetOpenTabsMode(OpenTabsMode mode) {
  open_tabs_mode_ = mode;
}

void StatusAreaView::CreateAppMenu() {
  if (app_menu_contents_.get())
    return;

  options_menu_contents_.reset(new OptionsMenuModel(browser_, this));

  app_menu_contents_.reset(new views::SimpleMenuModel(this));
  app_menu_contents_->AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  app_menu_contents_->AddItemWithStringId(IDC_NEW_WINDOW, IDS_NEW_WINDOW);
  app_menu_contents_->AddItemWithStringId(IDC_NEW_INCOGNITO_WINDOW,
                                          IDS_NEW_INCOGNITO_WINDOW);
  app_menu_contents_->AddSeparator();
  app_menu_contents_->AddCheckItemWithStringId(IDC_SHOW_BOOKMARK_BAR,
                                               IDS_SHOW_BOOKMARK_BAR);
  app_menu_contents_->AddItemWithStringId(IDC_FULLSCREEN, IDS_FULLSCREEN);
  app_menu_contents_->AddSeparator();
  app_menu_contents_->AddItemWithStringId(IDC_SHOW_HISTORY, IDS_SHOW_HISTORY);
  app_menu_contents_->AddItemWithStringId(IDC_SHOW_BOOKMARK_MANAGER,
                                          IDS_BOOKMARK_MANAGER);
  app_menu_contents_->AddItemWithStringId(IDC_SHOW_DOWNLOADS,
                                          IDS_SHOW_DOWNLOADS);
  app_menu_contents_->AddSeparator();
  app_menu_contents_->AddItemWithStringId(IDC_CLEAR_BROWSING_DATA,
                                          IDS_CLEAR_BROWSING_DATA);
  app_menu_contents_->AddItemWithStringId(IDC_IMPORT_SETTINGS,
                                          IDS_IMPORT_SETTINGS);
  app_menu_contents_->AddSeparator();
  app_menu_contents_->AddItem(IDC_OPTIONS,
                              l10n_util::GetStringFUTF16(
                                  IDS_OPTIONS,
                                  l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  app_menu_contents_->AddSubMenu(ASCIIToUTF16("Compact nav bar"),
                                 options_menu_contents_.get());
  app_menu_contents_->AddItem(IDC_ABOUT,
                              l10n_util::GetStringFUTF16(
                                  IDS_ABOUT,
                                  l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  app_menu_contents_->AddItemWithStringId(IDC_HELP_PAGE, IDS_HELP_PAGE);

  app_menu_menu_.reset(new views::Menu2(app_menu_contents_.get()));
}

bool StatusAreaView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool StatusAreaView::IsCommandIdEnabled(int command_id) const {
  if (command_id == IDC_RESTORE_TAB)
    return browser_->CanRestoreTab();
  return browser_->command_updater()->IsCommandEnabled(command_id);
}

bool StatusAreaView::GetAcceleratorForCommandId(
    int command_id,
    views::Accelerator* accelerator) {
  return false;
}

void StatusAreaView::ExecuteCommand(int command_id) {
  browser_->ExecuteCommand(command_id);
}

void StatusAreaView::RunMenu(views::View* source, const gfx::Point& pt) {
  CreateAppMenu();
  app_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}
