// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/tray_date.h"

#include "ash/shell.h"
#include "ash/system/date/date_view.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_views.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "unicode/datefmt.h"
#include "unicode/fieldpos.h"
#include "unicode/fmtable.h"

namespace {

const int kPaddingVertical = 10;

class DateDefaultView : public views::View,
                        public views::ButtonListener {
 public:
  explicit DateDefaultView(ash::user::LoginStatus login)
      : button_container_(NULL) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, 0));
    set_background(views::Background::CreateSolidBackground(
        ash::kHeaderBackgroundColor));

    ash::internal::tray::DateView* date_view =
        new ash::internal::tray::DateView();
    date_view->set_border(views::Border::CreateEmptyBorder(kPaddingVertical,
        ash::kTrayPopupPaddingHorizontal,
        kPaddingVertical,
        ash::kTrayPopupPaddingHorizontal));
    ash::internal::HoverHighlightView* view =
        new ash::internal::HoverHighlightView(NULL);
    view->SetLayoutManager(new views::FillLayout);
    view->set_focusable(false);
    view->set_highlight_color(SkColorSetARGB(0, 0, 0, 0));
    view->AddChildView(date_view);
    AddChildView(view);

    if (login == ash::user::LOGGED_IN_LOCKED ||
        login == ash::user::LOGGED_IN_NONE)
      return;

    date_view->SetActionable(true);
    view->set_highlight_color(ash::kHeaderHoverBackgroundColor);
    date_ = view;

    button_container_ = new views::View;
    button_container_->SetLayoutManager(new
        views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

    help_ = new ash::internal::TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_HELP,
        IDR_AURA_UBER_TRAY_HELP);
    button_container_->AddChildView(help_);

    shutdown_ = lock_ = NULL;

    if (login != ash::user::LOGGED_IN_LOCKED &&
        login != ash::user::LOGGED_IN_KIOSK) {
      shutdown_ = new ash::internal::TrayPopupHeaderButton(this,
          IDR_AURA_UBER_TRAY_SHUTDOWN,
          IDR_AURA_UBER_TRAY_SHUTDOWN);
      button_container_->AddChildView(shutdown_);

      if (login != ash::user::LOGGED_IN_GUEST) {
        lock_ = new ash::internal::TrayPopupHeaderButton(this,
            IDR_AURA_UBER_TRAY_LOCKSCREEN,
            IDR_AURA_UBER_TRAY_LOCKSCREEN);
        button_container_->AddChildView(lock_);
      }
    }
    AddChildView(button_container_);
  }

  virtual ~DateDefaultView() {}

 private:
  // Overridden from views::View.
  virtual void Layout() OVERRIDE {
    views::View::Layout();
    if (!button_container_)
      return;

    gfx::Rect bounds(button_container_->GetPreferredSize());
    bounds.set_height(height());
    bounds = gfx::Rect(size()).Center(bounds.size());
    bounds.set_x(width() - bounds.width());
    button_container_->SetBoundsRect(bounds);

    bounds = date_->bounds();
    bounds.set_width(button_container_->x());
    date_->SetBoundsRect(bounds);
  }

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    ash::SystemTrayDelegate* tray = ash::Shell::GetInstance()->tray_delegate();
    if (sender == help_)
      tray->ShowHelp();
    else if (sender == shutdown_)
      tray->ShutDown();
    else if (sender == lock_)
      tray->RequestLockScreen();
    else
      NOTREACHED();
  }

  views::View* button_container_;
  views::View* date_;
  views::ToggleImageButton* help_;
  views::ToggleImageButton* shutdown_;
  views::ToggleImageButton* lock_;

  DISALLOW_COPY_AND_ASSIGN(DateDefaultView);
};

}  // namespace

namespace ash {
namespace internal {

TrayDate::TrayDate()
    : time_tray_(NULL) {
}

TrayDate::~TrayDate() {
}

views::View* TrayDate::CreateTrayView(user::LoginStatus status) {
  CHECK(time_tray_ == NULL);
  time_tray_ = new tray::TimeView();
  time_tray_->set_border(
      views::Border::CreateEmptyBorder(0, 10, 0, 7));
  SetupLabelForTray(time_tray_->label());
  gfx::Font font = time_tray_->label()->font();
  time_tray_->label()->SetFont(
      font.DeriveFont(0, font.GetStyle() & ~gfx::Font::BOLD));

  views::View* view = new TrayItemView;
  view->AddChildView(time_tray_);
  return view;
}

views::View* TrayDate::CreateDefaultView(user::LoginStatus status) {
  return new DateDefaultView(status);
}

views::View* TrayDate::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayDate::DestroyTrayView() {
  time_tray_ = NULL;
}

void TrayDate::DestroyDefaultView() {
}

void TrayDate::DestroyDetailedView() {
}

void TrayDate::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayDate::OnDateFormatChanged() {
  if (time_tray_)
    time_tray_->UpdateTimeFormat();
}

void TrayDate::Refresh() {
  if (time_tray_)
    time_tray_->UpdateText();
}

}  // namespace internal
}  // namespace ash
