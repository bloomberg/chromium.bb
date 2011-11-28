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
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/status/status_area_bubble.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

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

// Color of text embedded within battery.
const SkColor kPercentageColor = 0xFF333333;
// Used for embossing text.
const SkColor kPercentageShadowColor = 0x80ffffff;
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
  const SkBitmap* all_images = ResourceBundle::GetSharedInstance().
      GetImageNamed(image_index).ToSkBitmap();
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

SkBitmap GetImageWithPercentage(ImageSize size, ImageType type,
                                double battery_percentage) {
  // Preserve the fully charged icon for 100% only.
  int battery_index = 0;
  if (battery_percentage >= 100) {
    battery_index = kNumPowerImages - 1;
  } else {
    battery_index = static_cast<int> (
        battery_percentage / 100.0 *
        nextafter(static_cast<double>(kNumPowerImages - 1), 0));
    battery_index =
        std::max(std::min(battery_index, kNumPowerImages - 2), 0);
  }
  return GetImage(size, type, battery_index);
}

SkBitmap GetUnknownImage(ImageSize size) {
  return GetImage(size, CHARGING, kNumPowerImages);
}

class BatteryIconView : public views::View {
 public:
  BatteryIconView()
      : battery_percentage_(0),
        battery_is_present_(false),
        line_power_on_(false),
        percentage_font_(ResourceBundle::GetSharedInstance().
                         GetFont(ResourceBundle::BaseFont).
                         DeriveFont(kBatteryFontSizeDelta, gfx::Font::BOLD)) {
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(kLargeImageWidth, kLargeImageHeight);
  }

  void set_battery_percentage(double battery_percentage) {
    battery_percentage_ = battery_percentage;
    SchedulePaint();
  }

  void set_battery_is_present(bool battery_is_present) {
    battery_is_present_ = battery_is_present;
    SchedulePaint();
  }

  void set_line_power_on(bool line_power_on) {
    line_power_on_ = line_power_on;
    SchedulePaint();
  }

 protected:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    SkBitmap image;
    if (battery_is_present_) {
      image = GetImageWithPercentage(LARGE,
                                     line_power_on_ ? CHARGING : DISCHARGING,
                                     battery_percentage_);
    } else {
      NOTREACHED();
      return;
    }
    const int image_x = 0;
    const int image_y = (height() - image.height()) / 2;
    canvas->DrawBitmapInt(image, image_x, image_y);

    if (battery_is_present_ && (battery_percentage_ < 100 || !line_power_on_)) {
      const string16 text = UTF8ToUTF16(base::StringPrintf(
          "%d%%", static_cast<int>(battery_percentage_)));
      const int text_h = percentage_font_.GetHeight();
      const int text_y = ((height() - text_h) / 2);
      canvas->DrawStringInt(
          text, percentage_font_, kPercentageShadowColor,
          image_x, text_y + 1, image.width(), text_h,
          gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_ELLIPSIS);
      canvas->DrawStringInt(
          text, percentage_font_, kPercentageColor,
          image_x, text_y, image.width(), text_h,
          gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_ELLIPSIS);
      if (line_power_on_)
        canvas->DrawBitmapInt(
            GetImageWithPercentage(LARGE, BOLT, battery_percentage_),
            image_x, image_y);
    }
  }

 private:
  double battery_percentage_;
  bool battery_is_present_;
  bool line_power_on_;
  gfx::Font percentage_font_;

  DISALLOW_COPY_AND_ASSIGN(BatteryIconView);
};

}  // namespace

namespace chromeos {

using base::TimeDelta;

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton

PowerMenuButton::PowerMenuButton(StatusAreaButton::Delegate* delegate)
    : StatusAreaButton(delegate, this),
      battery_is_present_(false),
      line_power_on_(false),
      battery_percentage_(0.0),
      battery_time_to_full_(TimeDelta::FromMicroseconds(kInitialMS)),
      battery_time_to_empty_(TimeDelta::FromMicroseconds(kInitialMS)),
      status_(NULL) {
  set_id(VIEW_ID_STATUS_BUTTON_POWER);
  UpdateIconAndLabelInfo();
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate();
}

PowerMenuButton::~PowerMenuButton() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

// PowerMenuButton, views::MenuDelegate implementation:

string16 PowerMenuButton::GetLabel(int id) const {
  return string16();
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
  // Explicitly query the power status.
  DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate();

  views::MenuItemView* menu = new views::MenuItemView(this);
  // MenuRunner takes ownership of |menu|.
  menu_runner_.reset(new views::MenuRunner(menu));
  views::MenuItemView* submenu = menu->AppendMenuItem(
      POWER_BATTERY_PERCENTAGE_ITEM,
      string16(),
      views::MenuItemView::NORMAL);
  status_ = new StatusAreaBubbleContentView(new BatteryIconView, string16());
  UpdateStatusView();
  submenu->AddChildView(status_);
  menu->CreateSubmenu()->set_resize_open_menu(true);
  menu->SetMargins(0, 0);
  submenu->SetMargins(0, 0);
  menu->ChildrenChanged();

  gfx::Point screen_location;
  views::View::ConvertPointToScreen(source, &screen_location);
  gfx::Rect bounds(screen_location, source->size());
  if (menu_runner_->RunMenuAt(
          source->GetWidget()->GetTopLevelWidget(), this, bounds,
          views::MenuItemView::TOPRIGHT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
  status_ = NULL;
  menu_runner_.reset(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, PowerManagerClient::Observer implementation:

void PowerMenuButton::PowerChanged(const PowerSupplyStatus& power_status) {
  power_status_ = power_status;
  UpdateIconAndLabelInfo();
}

////////////////////////////////////////////////////////////////////////////////
// PowerMenuButton, StatusAreaButton implementation:

void PowerMenuButton::UpdateIconAndLabelInfo() {
  battery_is_present_ = power_status_.battery_is_present;
  line_power_on_ = power_status_.line_power_on;

  bool should_be_visible = battery_is_present_;
  if (should_be_visible != IsVisible())
    SetVisible(should_be_visible);

  if (!should_be_visible)
    return;

  // If fully charged, always show 100% even if internal number is a bit less.
  if (power_status_.battery_is_full)
    battery_percentage_ = 100.0;
  else
    battery_percentage_ = power_status_.battery_percentage;

  UpdateBatteryTime(&battery_time_to_full_,
                    TimeDelta::FromSeconds(
                        power_status_.battery_seconds_to_full));
  UpdateBatteryTime(&battery_time_to_empty_,
                    TimeDelta::FromSeconds(
                        power_status_.battery_seconds_to_empty));

  SetIcon(GetImageWithPercentage(
      SMALL, line_power_on_ ? CHARGING : DISCHARGING, battery_percentage_));
  const int message_id = line_power_on_ ?
      IDS_STATUSBAR_BATTERY_CHARGING_PERCENTAGE :
      IDS_STATUSBAR_BATTERY_USING_PERCENTAGE;
  string16 tooltip_text =  l10n_util::GetStringFUTF16(
       message_id, base::IntToString16(static_cast<int>(battery_percentage_)));
  SetTooltipText(tooltip_text);
  SetAccessibleName(tooltip_text);
  SchedulePaint();
  UpdateStatusView();
}

void PowerMenuButton::UpdateStatusView() {
  if (status_) {
    string16 charging_text;
    if (battery_is_present_) {
      charging_text = GetBatteryIsChargedText();
    } else {
      charging_text = l10n_util::GetStringUTF16(IDS_STATUSBAR_NO_BATTERY);
    }
    status_->SetMessage(charging_text);
    BatteryIconView* battery_icon_view =
        static_cast<BatteryIconView*>(status_->icon_view());
    battery_icon_view->set_battery_percentage(battery_percentage_);
    battery_icon_view->set_battery_is_present(battery_is_present_);
    battery_icon_view->set_line_power_on(line_power_on_);
  }
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
