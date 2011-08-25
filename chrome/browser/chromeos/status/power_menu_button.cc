// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/power_menu_button.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/menu_runner.h"
#include "views/controls/menu/submenu_view.h"
#include "views/widget/widget.h"

namespace {

// Menu item ids.
enum {
  POWER_BATTERY_PERCENTAGE_ITEM = 1000,
  POWER_BATTERY_IS_CHARGED_ITEM,
  POWER_NO_BATTERY,
};

enum ImageType {
  DISCHARGING,
  CHARGING,
  BOLT
};

enum ImageSize {
  SMALL,
  LARGE
};

// Initialize time deltas to large values so they will be replaced when set
// to small values.
const int64 kInitialMS = 0x7fffffff;
// Width and height of small images.
const int kSmallImageWidth = 26, kSmallImageHeight = 24;
// Width and height of large images.
const int kLargeImageWidth = 57, kLargeImageHeight = 35;
// Number of different power states.
const int kNumPowerImages = 20;

// Constants for status displayed when user clicks button.
// Padding around status.
const int kPadLeftX = 10, kPadRightX = 10, kPadY = 5;
// Padding between battery and text.
const int kBatteryPadX = 10;
// Spacing between lines of text.
const int kEstimateSpacing = 3;
// Color of text embedded within battery.
const SkColor kPercentageColor = 0xFF333333;
// Used for embossing text.
const SkColor kPercentageShadowColor = 0x80ffffff;
// Status text/
const SkColor kEstimateColor = SK_ColorBLACK;
// Size of percentage w/in battery.
const int kBatteryFontSizeDelta = 3;

// Battery images come from two collections (small and large). In each there
// are |kNumPowerImages| battery states for both on and off charge, followed
// by the missing battery image and the unknown image.
// They are layed out like this:
// Discharging Charging Bolt
// |           |        +
// ||          ||        +
// ...
// |||||       |||||        +
// ||X||       ||?||
SkBitmap GetImage(ImageSize size, ImageType type, int index) {
  int image_width, image_height, image_index;

  if (size == SMALL) {
    image_index = IDR_STATUSBAR_BATTERY_SMALL_ALL;
    image_width = kSmallImageWidth;
    image_height = kSmallImageHeight;
  } else {
    image_index = IDR_STATUSBAR_BATTERY_LARGE_ALL;
    image_width = kLargeImageWidth;
    image_height = kLargeImageHeight;
  }
  SkBitmap* all_images =
      ResourceBundle::GetSharedInstance().GetBitmapNamed(image_index);
  SkIRect subset =
      SkIRect::MakeXYWH(
          static_cast<int>(type) * image_width,
          index * image_height,
          image_width,
          image_height);

  SkBitmap image;
  all_images->extractSubset(&image, subset);
  return image;
}

SkBitmap GetMissingImage(ImageSize size) {
  return GetImage(size, DISCHARGING, kNumPowerImages);
}

SkBitmap GetUnknownImage(ImageSize size) {
  return GetImage(size, CHARGING, kNumPowerImages);
}

}  // namespace

namespace chromeos {

using base::TimeDelta;

class PowerMenuButton::StatusView : public View {
 public:
  explicit StatusView(PowerMenuButton* menu_button)
      : menu_button_(menu_button) {
    estimate_font_ =
        ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
    percentage_font_ =
        estimate_font_.DeriveFont(kBatteryFontSizeDelta, gfx::Font::BOLD);
  }

  gfx::Size GetPreferredSize() {
    int estimate_w, estimate_h, charging_w, charging_h;
    string16 estimate_text = menu_button_->GetBatteryIsChargedText();
    string16 charging_text = l10n_util::GetStringUTF16(
        menu_button_->line_power_on_ ?
            IDS_STATUSBAR_BATTERY_CHARGING :
            IDS_STATUSBAR_BATTERY_DISCHARGING);
    gfx::CanvasSkia::SizeStringInt(
        estimate_text, estimate_font_, &estimate_w, &estimate_h, 0);
    gfx::CanvasSkia::SizeStringInt(
        charging_text, estimate_font_, &charging_w, &charging_h, 0);
    gfx::Size size = gfx::Size(
        kPadLeftX + kLargeImageWidth + kBatteryPadX + kPadRightX +
            std::max(charging_w, estimate_w),
        (2 * kPadY) +
            std::max(kLargeImageHeight,
                     kEstimateSpacing + (2 * estimate_font_.GetHeight())));
    return size;
  }

  void Update() {
    PreferredSizeChanged();
    // Force a paint even if the size didn't change.
    SchedulePaint();
  }

 protected:
  void OnPaint(gfx::Canvas* canvas) {
    SkBitmap image;

    bool draw_percentage_text = false;
    if (!CrosLibrary::Get()->EnsureLoaded()) {
      image = GetUnknownImage(LARGE);
    } else if (!menu_button_->battery_is_present_) {
      image = GetMissingImage(LARGE);
    } else {
      image = GetImage(
          LARGE,
          menu_button_->line_power_on_ ? CHARGING : DISCHARGING,
          menu_button_->battery_index_);
      if (menu_button_->battery_percentage_ < 100 ||
          !menu_button_->line_power_on_) {
        draw_percentage_text = true;
      }
    }
    int image_x = kPadLeftX, image_y = (height() - image.height()) / 2;
    canvas->DrawBitmapInt(image, image_x, image_y);

    if (draw_percentage_text) {
      string16 text = UTF8ToUTF16(base::StringPrintf(
          "%d%%",
          static_cast<int>(menu_button_->battery_percentage_)));
      int text_h = percentage_font_.GetHeight();
      int text_y = ((height() - text_h) / 2);
      canvas->DrawStringInt(
          text, percentage_font_, kPercentageShadowColor,
          image_x, text_y + 1, image.width(), text_h,
          gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_ELLIPSIS);
      canvas->DrawStringInt(
          text, percentage_font_, kPercentageColor,
          image_x, text_y, image.width(), text_h,
          gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_ELLIPSIS);
      if (menu_button_->line_power_on_) {
        image = GetImage(LARGE, BOLT, menu_button_->battery_index_);
        canvas->DrawBitmapInt(image, image_x, image_y);
      }
    }
    string16 charging_text = l10n_util::GetStringUTF16(
        menu_button_->line_power_on_ ?
            IDS_STATUSBAR_BATTERY_CHARGING :
            IDS_STATUSBAR_BATTERY_DISCHARGING);
    string16 estimate_text = menu_button_->GetBatteryIsChargedText();
    int text_h = estimate_font_.GetHeight();
    int text_x = image_x + kLargeImageWidth + kBatteryPadX;
    int charging_y = (height() - (kEstimateSpacing + (2 * text_h))) / 2;
    int estimate_y = charging_y + text_h + kEstimateSpacing;
    canvas->DrawStringInt(
        charging_text, estimate_font_, kEstimateColor,
        text_x, charging_y, width() - text_x, text_h,
        gfx::Canvas::TEXT_ALIGN_LEFT);
    canvas->DrawStringInt(
        estimate_text, estimate_font_, kEstimateColor,
        text_x, estimate_y, width() - text_x, text_h,
        gfx::Canvas::TEXT_ALIGN_LEFT);
  }

  bool OnMousePressed(const views::MouseEvent& event) {
    return true;
  }

  void OnMouseReleased(const views::MouseEvent& event) {
    if (event.IsLeftMouseButton()) {
      DCHECK(menu_button_->menu_runner_);
      menu_button_->menu_runner_->Cancel();
    }
  }

 private:
  PowerMenuButton* menu_button_;
  gfx::Font percentage_font_;
  gfx::Font estimate_font_;
};

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton

PowerMenuButton::PowerMenuButton(StatusAreaHost* host)
    : StatusAreaButton(host, this),
      battery_is_present_(false),
      line_power_on_(false),
      battery_percentage_(0.0),
      battery_index_(-1),
      battery_time_to_full_(TimeDelta::FromMicroseconds(kInitialMS)),
      battery_time_to_empty_(TimeDelta::FromMicroseconds(kInitialMS)),
      status_(NULL),
      menu_runner_(NULL) {
  UpdateIconAndLabelInfo();
  CrosLibrary::Get()->GetPowerLibrary()->AddObserver(this);
}

PowerMenuButton::~PowerMenuButton() {
  CrosLibrary::Get()->GetPowerLibrary()->RemoveObserver(this);
}

// PowerMenuButton, views::MenuDelegate implementation:

std::wstring PowerMenuButton::GetLabel(int id) const {
  return std::wstring();
}

bool PowerMenuButton::IsCommandEnabled(int id) const {
  return false;
}

string16 PowerMenuButton::GetBatteryPercentageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_STATUSBAR_BATTERY_PERCENTAGE,
      base::IntToString16(static_cast<int>(battery_percentage_)));
}

string16 PowerMenuButton::GetBatteryIsChargedText() const {
  // The second item shows the battery is charged if it is.
  if (battery_percentage_ >= 100 && line_power_on_)
    return l10n_util::GetStringUTF16(IDS_STATUSBAR_BATTERY_IS_CHARGED);

  // If battery is in an intermediate charge state, show how much time left.
  TimeDelta time = line_power_on_ ? battery_time_to_full_ :
      battery_time_to_empty_;
  if (time.InSeconds() == 0) {
    // If time is 0, then that means we are still calculating how much time.
    // Depending if line power is on, we either show a message saying that we
    // are calculating time until full or calculating remaining time.
    int msg = line_power_on_ ?
        IDS_STATUSBAR_BATTERY_CALCULATING_TIME_UNTIL_FULL :
        IDS_STATUSBAR_BATTERY_CALCULATING_TIME_UNTIL_EMPTY;
    return l10n_util::GetStringUTF16(msg);
  } else {
    // Depending if line power is on, we either show a message saying XX:YY
    // until full or XX:YY remaining where XX is number of hours and YY is
    // number of minutes.
    int msg = line_power_on_ ? IDS_STATUSBAR_BATTERY_TIME_UNTIL_FULL :
        IDS_STATUSBAR_BATTERY_TIME_UNTIL_EMPTY;
    int hour = time.InHours();
    int min = (time - TimeDelta::FromHours(hour)).InMinutes();
    string16 hour_str = base::IntToString16(hour);
    string16 min_str = base::IntToString16(min);
    // Append a "0" before the minute if it's only a single digit.
    if (min < 10)
      min_str = ASCIIToUTF16("0") + min_str;
    return l10n_util::GetStringFUTF16(msg, hour_str, min_str);
  }
}

int PowerMenuButton::icon_width() {
  return 26;
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, views::View implementation:
void PowerMenuButton::OnLocaleChanged() {
  UpdateIconAndLabelInfo();
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, views::ViewMenuDelegate implementation:

void PowerMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  views::MenuItemView* menu = new views::MenuItemView(this);
  // MenuRunner takes ownership of |menu|.
  views::MenuRunner menu_runner(menu);
  views::MenuItemView* submenu = menu->AppendMenuItem(
          POWER_BATTERY_PERCENTAGE_ITEM,
          std::wstring(),
          views::MenuItemView::NORMAL);
  status_ = new StatusView(this);
  submenu->AddChildView(status_);
  menu->CreateSubmenu()->set_resize_open_menu(true);
  menu->SetMargins(0, 0);
  submenu->SetMargins(0, 0);
  menu->ChildrenChanged();

  gfx::Point screen_location;
  views::View::ConvertPointToScreen(source, &screen_location);
  gfx::Rect bounds(screen_location, source->size());
  AutoReset<views::MenuRunner*> menu_runner_reseter(&menu_runner_,
                                                    &menu_runner);
  if (menu_runner.RunMenuAt(
          source->GetWidget()->GetTopLevelWidget(), this, bounds,
          views::MenuItemView::TOPRIGHT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
  status_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, PowerLibrary::Observer implementation:

void PowerMenuButton::PowerChanged(PowerLibrary* obj) {
  UpdateIconAndLabelInfo();
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, StatusAreaButton implementation:

void PowerMenuButton::UpdateIconAndLabelInfo() {
  PowerLibrary* cros = CrosLibrary::Get()->GetPowerLibrary();
  if (!cros)
    return;

  bool cros_loaded = CrosLibrary::Get()->EnsureLoaded();
  if (cros_loaded) {
    battery_is_present_ = cros->battery_is_present();
    line_power_on_ = cros->line_power_on();

    // If fully charged, always show 100% even if internal number is a bit less.
    if (cros->battery_fully_charged()) {
      // We always call cros->battery_percentage() for test predictability.
      cros->battery_percentage();
      battery_percentage_ = 100.0;
    } else {
      battery_percentage_ = cros->battery_percentage();
    }

    UpdateBatteryTime(&battery_time_to_full_, cros->battery_time_to_full());
    UpdateBatteryTime(&battery_time_to_empty_, cros->battery_time_to_empty());
  }

  if (!cros_loaded) {
    battery_index_ = -1;
    SetIcon(GetUnknownImage(SMALL));
  } else if (!battery_is_present_) {
    battery_index_ = -1;
    SetIcon(GetMissingImage(SMALL));
  } else {
    // Preserve the fully charged icon for 100% only.
    if (battery_percentage_ >= 100) {
      battery_index_ = kNumPowerImages - 1;
    } else {
      battery_index_ =
          static_cast<int>(battery_percentage_ / 100.0 *
              nextafter(static_cast<float>(kNumPowerImages - 1), 0));
      battery_index_ =
          std::max(std::min(battery_index_, kNumPowerImages - 2), 0);
    }
    SetIcon(GetImage(
        SMALL, line_power_on_ ? CHARGING : DISCHARGING, battery_index_));
  }

  percentage_text_ = GetBatteryPercentageText();
  SetTooltipText(UTF16ToWide(percentage_text_));
  SetAccessibleName(percentage_text_);
  SchedulePaint();
  if (status_)
    status_->Update();
}

void PowerMenuButton::UpdateBatteryTime(TimeDelta* previous,
                                        const TimeDelta& current) {
  static const TimeDelta kMaxDiff(TimeDelta::FromMinutes(10));
  static const TimeDelta kMinDiff(TimeDelta::FromMinutes(0));
  const TimeDelta diff = current - *previous;
  // If the diff is small and positive, ignore it in favor of
  // keeping time monotonically decreasing.
  // If previous is 0, then it either was never set (initial condition)
  // or got down to 0.
  if (*previous == TimeDelta::FromMicroseconds(kInitialMS) ||
      diff < kMinDiff ||
      diff > kMaxDiff) {
    *previous = current;
  }
}

}  // namespace chromeos
