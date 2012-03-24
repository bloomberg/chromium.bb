// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/tray_date.h"

#include "ash/shell.h"
#include "ash/system/date/date_view.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
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
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "unicode/datefmt.h"
#include "unicode/fieldpos.h"
#include "unicode/fmtable.h"

namespace ash {
namespace internal {

TrayDate::TrayDate() {
}

TrayDate::~TrayDate() {
}

views::View* TrayDate::CreateTrayView(user::LoginStatus status) {
  date_tray_.reset(new tray::DateView(tray::DateView::TIME));
  SetupLabelForTray(date_tray_->label());
  gfx::Font font = date_tray_->label()->font();
  date_tray_->label()->SetFont(
      font.DeriveFont(0, font.GetStyle() & ~gfx::Font::BOLD));
  return date_tray_.get();
}

views::View* TrayDate::CreateDefaultView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayDate::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayDate::DestroyTrayView() {
  date_tray_.reset();
}

void TrayDate::DestroyDefaultView() {
}

void TrayDate::DestroyDetailedView() {
}

void TrayDate::OnDateFormatChanged() {
  date_tray_->UpdateTimeFormat();
}

void TrayDate::Refresh() {
  date_tray_->UpdateText();
}

}  // namespace internal
}  // namespace ash
