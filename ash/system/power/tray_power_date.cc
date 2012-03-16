// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tray_power_date.h"

#include "ash/shell.h"
#include "ash/system/power/power_supply_status.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "base/i18n/time_formatting.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "unicode/datefmt.h"
#include "unicode/fieldpos.h"
#include "unicode/fmtable.h"

namespace ash {
namespace internal {

namespace {
// Width and height of battery images.
const int kBatteryImageHeight = 26;
const int kBatteryImageWidth = 24;
// Number of different power states.
const int kNumPowerImages = 20;
// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

string16 FormatNicely(const base::Time& time) {
  icu::UnicodeString date_string;

  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kFull));
  icu::FieldPosition position;
  position.setField(UDAT_DAY_OF_WEEK_FIELD);
  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000),
                    date_string,
                    position);
  icu::UnicodeString day = date_string.retainBetween(position.getBeginIndex(),
                                                     position.getEndIndex());

  date_string.remove();
  formatter.reset(
      icu::DateFormat::createDateInstance(icu::DateFormat::kMedium));
  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);

  date_string += "\n";
  date_string += day;

  return string16(date_string.getBuffer(),
                  static_cast<size_t>(date_string.length()));
}

}

namespace tray {

// This view is used for both the tray and the popup.
class DateView : public views::View {
 public:
  enum TimeType {
    TIME,
    DATE
  };

  explicit DateView(TimeType type)
      : hour_type_(ash::Shell::GetInstance()->tray_delegate()->
            GetHourClockType()),
        type_(type),
        actionable_(false) {
    SetLayoutManager(new views::FillLayout());
    label_ = new views::Label;
    label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    UpdateText();
    AddChildView(label_);
  }

  virtual ~DateView() {
    timer_.Stop();
  }

  void UpdateTimeFormat() {
    hour_type_ = ash::Shell::GetInstance()->tray_delegate()->GetHourClockType();
    UpdateText();
  }

  views::Label* label() const { return label_; }

  void set_actionable(bool actionable) { actionable_ = actionable; }

 private:
  void UpdateText() {
    base::Time now = base::Time::Now();
    if (type_ == DATE) {
      label_->SetText(FormatNicely(now));
    } else {
      label_->SetText(base::TimeFormatTimeOfDayWithHourClockType(
          now, hour_type_, base::kDropAmPm));
    }

    label_->SetTooltipText(base::TimeFormatFriendlyDate(now));
    SchedulePaint();

    // Try to set the timer to go off at the next change of the minute. We don't
    // want to have the timer go off more than necessary since that will cause
    // the CPU to wake up and consume power.
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

    timer_.Stop();
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(seconds_left), this,
        &DateView::UpdateText);
  }

  // Overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    if (!actionable_)
      return false;

    ash::Shell::GetInstance()->tray_delegate()->ShowDateSettings();
    return true;
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    if (!actionable_)
      return;
    gfx::Font font = label_->font();
    label_->SetFont(font.DeriveFont(0,
          font.GetStyle() | gfx::Font::UNDERLINED));
    SchedulePaint();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    if (!actionable_)
      return;
    gfx::Font font = label_->font();
    label_->SetFont(font.DeriveFont(0,
          font.GetStyle() & ~gfx::Font::UNDERLINED));
    SchedulePaint();
  }

  virtual void OnLocaleChanged() OVERRIDE {
    UpdateText();
  }

  base::OneShotTimer<DateView> timer_;
  base::HourClockType hour_type_;
  TimeType type_;
  bool actionable_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DateView);
};

// This view is used only for the tray.
class PowerTrayView : public views::ImageView {
 public:
  PowerTrayView() {
    UpdateImage();
  }

  virtual ~PowerTrayView() {
  }

  void UpdatePowerStatus(const PowerSupplyStatus& status) {
    supply_status_ = status;
    // Sanitize.
    if (supply_status_.battery_is_full)
      supply_status_.battery_percentage = 100.0;

    UpdateImage();
  }

 private:
  void UpdateImage() {
    SkBitmap image;
    gfx::Image all = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_POWER_SMALL);

    int image_index = 0;
    if (supply_status_.battery_percentage >= 100) {
      image_index = kNumPowerImages - 1;
    } else if (!supply_status_.battery_is_present) {
      image_index = kNumPowerImages;
    } else {
      image_index = static_cast<int> (
          supply_status_.battery_percentage / 100.0 *
          (kNumPowerImages - 1));
      image_index =
        std::max(std::min(image_index, kNumPowerImages - 2), 0);
    }

    SkIRect region = SkIRect::MakeXYWH(
        image_index * kBatteryImageWidth,
        supply_status_.line_power_on ? 0 : kBatteryImageHeight,
        kBatteryImageWidth, kBatteryImageHeight);
    all.ToSkBitmap()->extractSubset(&image, region);

    SetImage(image);
  }

  PowerSupplyStatus supply_status_;

  DISALLOW_COPY_AND_ASSIGN(PowerTrayView);
};

// This view is used only for the popup.
class PowerPopupView : public views::Label {
 public:
  PowerPopupView() {
    SetHorizontalAlignment(ALIGN_RIGHT);
    UpdateText();
  }

  virtual ~PowerPopupView() {
  }

  void UpdatePowerStatus(const PowerSupplyStatus& status) {
    supply_status_ = status;
    // Sanitize.
    if (supply_status_.battery_is_full)
      supply_status_.battery_percentage = 100.0;

    UpdateText();
  }

 private:
  void UpdateText() {
    base::TimeDelta time = base::TimeDelta::FromSeconds(
        supply_status_.line_power_on ?
        supply_status_.battery_seconds_to_full :
        supply_status_.battery_seconds_to_empty);
    int hour = time.InHours();
    int min = (time - base::TimeDelta::FromHours(hour)).InMinutes();
    // TODO: Translation
    SetText(ASCIIToUTF16(base::StringPrintf("Battery: %.0lf%%\n%dh%02dm",
          supply_status_.battery_percentage,
          hour, min)));
  }

  PowerSupplyStatus supply_status_;

  DISALLOW_COPY_AND_ASSIGN(PowerPopupView);
};

}  // namespace tray

TrayPowerDate::TrayPowerDate()
    : power_(NULL),
      power_tray_(NULL) {
}

TrayPowerDate::~TrayPowerDate() {
}

views::View* TrayPowerDate::CreateTrayView(user::LoginStatus status) {
  views::View* container = new views::View;
  container->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, kTrayPaddingBetweenItems));

  PowerSupplyStatus power_status =
      ash::Shell::GetInstance()->tray_delegate()->GetPowerSupplyStatus();
  if (power_status.battery_is_present) {
    power_tray_.reset(new tray::PowerTrayView());
    power_tray_->UpdatePowerStatus(power_status);
    container->AddChildView(power_tray_.get());
  }

  date_tray_.reset(new tray::DateView(tray::DateView::TIME));
  date_tray_->label()->SetFont(
      date_tray_->label()->font().DeriveFont(2, gfx::Font::BOLD));
  date_tray_->label()->SetAutoColorReadabilityEnabled(false);
  date_tray_->label()->SetEnabledColor(SK_ColorWHITE);
  container->AddChildView(date_tray_.get());

  return container;
}

views::View* TrayPowerDate::CreateDefaultView(user::LoginStatus status) {
  date_.reset(new tray::DateView(tray::DateView::DATE));
  if (status != user::LOGGED_IN_NONE)
    date_->set_actionable(true);

  views::View* container = new views::View;
  views::BoxLayout* layout = new
      views::BoxLayout(views::BoxLayout::kHorizontal, 18, 10, 0);
  layout->set_spread_blank_space(true);
  container->SetLayoutManager(layout);
  container->set_background(views::Background::CreateSolidBackground(
      SkColorSetRGB(245, 245, 245)));
  container->AddChildView(date_.get());

  PowerSupplyStatus power_status =
      ash::Shell::GetInstance()->tray_delegate()->GetPowerSupplyStatus();
  if (power_status.battery_is_present) {
    power_.reset(new tray::PowerPopupView());
    power_->UpdatePowerStatus(power_status);
    container->AddChildView(power_.get());
  }
  return container;
}

views::View* TrayPowerDate::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayPowerDate::DestroyTrayView() {
  date_tray_.reset();
  power_tray_.reset();
}

void TrayPowerDate::DestroyDefaultView() {
  date_.reset();
  power_.reset();
}

void TrayPowerDate::DestroyDetailedView() {
}

void TrayPowerDate::OnPowerStatusChanged(const PowerSupplyStatus& status) {
  if (power_tray_.get())
    power_tray_->UpdatePowerStatus(status);
  if (power_.get())
    power_->UpdatePowerStatus(status);
}

void TrayPowerDate::OnDateFormatChanged() {
  date_tray_->UpdateTimeFormat();
}

}  // namespace internal
}  // namespace ash
