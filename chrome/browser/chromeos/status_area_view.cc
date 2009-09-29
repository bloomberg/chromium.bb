// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status_area_view.h"

#include <dlfcn.h>

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/network_menu_button.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/image_view.h"
#include "views/controls/menu/menu.h"
#include "views/controls/menu/simple_menu_model.h"

namespace {

// The number of images representing the fullness of the battery.
const int kNumBatteryImages = 8;

// Number of pixels to separate adjacent status items.
const int kStatusItemSeparation = 1;

// BrowserWindowGtk tiles its image with this offset
const int kCustomFrameBackgroundVerticalOffset = 15;

class ClockView : public views::View {
 public:
  ClockView();
  virtual ~ClockView();

  // views::View* overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Paint(gfx::Canvas* canvas);

 private:
  // Schedules the timer to fire at the next minute interval.
  void SetNextTimer();

  // Schedules a paint when the timer goes off.
  void OnTimer();

  gfx::Font font_;

  base::OneShotTimer<ClockView> timer_;

  DISALLOW_COPY_AND_ASSIGN(ClockView);
};

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

ClockView::ClockView()
    : font_(ResourceBundle::GetSharedInstance().GetFont(
                ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD)) {
  SetNextTimer();
}

ClockView::~ClockView() {
}

gfx::Size ClockView::GetPreferredSize() {
  return gfx::Size(40, 12);
}

void ClockView::Paint(gfx::Canvas* canvas) {
  base::Time now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);
  int hour = now_exploded.hour % 12;
  if (hour == 0)
    hour = 12;

  std::wstring time_string = StringPrintf(L"%d:%02d%lc",
                                          hour,
                                          now_exploded.minute,
                                          now_exploded.hour < 12 ? L'p' : L'p');
  canvas->DrawStringInt(time_string, font_, SK_ColorWHITE, 0, 0,
                        width(), height(), gfx::Canvas::TEXT_ALIGN_CENTER);
}

void ClockView::SetNextTimer() {
  // Try to set the timer to go off at the next change of the minute. We don't
  // want to have the timer go off more than necessary since that will cause
  // the CPU to wake up and consume power.
  base::Time now = base::Time::Now();
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);

  // Often this will be called at minute boundaries, and we'll actually want
  // 60 seconds from now.
  int seconds_left = 60 - exploded.second;
  if (seconds_left == 0)
    seconds_left = 60;

  // Make sure that the timer fires on the next minute. Without this, if it is
  // called just a teeny bit early, then it will skip the next minute.
  seconds_left += kTimerSlopSeconds;

  timer_.Start(base::TimeDelta::FromSeconds(seconds_left),
               this, &ClockView::OnTimer);
}

void ClockView::OnTimer() {
  SchedulePaint();
  SetNextTimer();
}

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
        // Reach into the GTK browser window and enable the flag to create the
        // next window as a compact nav one.
        // TODO(brettw) this is an evil hack, and is here so this can be tested.
        // Remove it eventually.
        static_cast<BrowserWindowGtk*>(browser_->window())->
            set_next_window_should_use_compact_nav();
        browser_->ExecuteCommand(IDC_NEW_WINDOW);
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

// static
void* StatusAreaView::power_library_ = NULL;
bool StatusAreaView::power_library_error_ = false;

StatusAreaView::StatusAreaView(Browser* browser)
    : browser_(browser),
      network_view_(NULL),
      battery_view_(NULL),
      menu_view_(NULL),
      power_status_connection_(NULL) {
}

StatusAreaView::~StatusAreaView() {
  if (power_status_connection_)
    chromeos::DisconnectPowerStatus(power_status_connection_);
}

void StatusAreaView::Init() {
  LoadPowerLibrary();
  ThemeProvider* theme = browser_->profile()->GetThemeProvider();

  // Clock.
  AddChildView(new ClockView);

  // Network.
  network_view_ = new NetworkMenuButton(browser_);
  AddChildView(network_view_);

  // Battery.
  battery_view_ = new views::ImageView;
  battery_view_->SetImage(theme->GetBitmapNamed(IDR_STATUSBAR_BATTERY_UNKNOWN));
  AddChildView(battery_view_);

  // Menu.
  menu_view_ = new views::MenuButton(NULL, std::wstring(), this, false);
  menu_view_->SetIcon(*theme->GetBitmapNamed(IDR_STATUSBAR_MENU));
  AddChildView(menu_view_);

  if (power_library_) {
    power_status_connection_ = chromeos::MonitorPowerStatus(
        StatusAreaView::PowerStatusChangedHandler,
        this);
  }
}

gfx::Size StatusAreaView::GetPreferredSize() {
  int result_w = kStatusItemSeparation;  // Left border.
  int result_h = 0;
  for (int i = 0; i < GetChildViewCount(); i++) {
    gfx::Size cur_size = GetChildViewAt(i)->GetPreferredSize();
    result_w += cur_size.width() + kStatusItemSeparation;
    result_h = std::max(result_h, cur_size.height());
  }
  result_w -= kStatusItemSeparation;  // Don't have space after the last one.

  // TODO(brettw) do we need to use result_h? This is currently hardcoded
  // becuase the menu button really wants to be larger, but we don't want
  // the status bar to force the whole tab strip to be larger. Making it
  // "smaller" just means that we'll expand to the height, which we want.
  // The height of 24 makes it just big enough to sit on top of the shadow
  // drawn by the browser window.
  return gfx::Size(result_w - kStatusItemSeparation, 24);
}

void StatusAreaView::Layout() {
  int cur_x = 0;
  for (int i = 0; i < GetChildViewCount(); i++) {
    views::View* cur = GetChildViewAt(i);
    gfx::Size cur_size = cur->GetPreferredSize();

    // Put next in row horizontally, and center vertically.
    // Padding the y position by 1 balances the space above and
    // below the icons, but still allows the shadow to be drawn.
    cur->SetBounds(cur_x, (height() - cur_size.height()) / 2 + 1,
                   cur_size.width(), cur_size.height());
    cur_x += cur_size.width() + kStatusItemSeparation;
  }
}

void StatusAreaView::Paint(gfx::Canvas* canvas) {
  ThemeProvider* theme = browser_->profile()->GetThemeProvider();

  // Fill the background.
  SkBitmap* background;
  if (browser_->window()->IsActive())
    background = theme->GetBitmapNamed(IDR_THEME_FRAME);
  else
    background = theme->GetBitmapNamed(IDR_THEME_FRAME_INACTIVE);
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

void StatusAreaView::RunMenu(views::View* source, const gfx::Point& pt,
                             gfx::NativeView hwnd) {
  CreateAppMenu();
  app_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

// static
void StatusAreaView::LoadPowerLibrary() {
  if (power_library_) {
    // Already loaded.
    return;
  }

  if (power_library_error_) {
    // Error in previous load attempt.
    return;
  }

  FilePath path;
  if (PathService::Get(chrome::FILE_CHROMEOS_API, &path)) {
    power_library_ = dlopen(path.value().c_str(), RTLD_NOW);
    if (power_library_) {
      chromeos::LoadCros(power_library_);
    } else {
      power_library_error_ = true;
      char* error = dlerror();
      if (error) {
        LOG(ERROR) << "Problem loading chromeos shared object: " << error;
      }
    }
  }
}

// static
void StatusAreaView::PowerStatusChangedHandler(
    void* object, const chromeos::PowerStatus& status) {
  static_cast<StatusAreaView*>(object)->PowerStatusChanged(status);
}

void StatusAreaView::PowerStatusChanged(const chromeos::PowerStatus& status) {
  ThemeProvider* theme = browser_->profile()->GetThemeProvider();
  int image_index;

  if (status.battery_state == chromeos::BATTERY_STATE_FULLY_CHARGED &&
      status.line_power_on) {
    image_index = IDR_STATUSBAR_BATTERY_CHARGED;
  } else {
    image_index = status.line_power_on ?
        IDR_STATUSBAR_BATTERY_CHARGING_1 :
        IDR_STATUSBAR_BATTERY_DISCHARGING_1;
    double percentage = status.battery_percentage;
    int offset = floor(percentage / (100.0 / kNumBatteryImages));
    // This can happen if the battery is 100% full.
    if (offset == kNumBatteryImages)
      offset--;
    image_index += offset;
  }
  battery_view_->SetImage(theme->GetBitmapNamed(image_index));
}
