// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/tray_tracing.h"

#include "ash/shell.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace tray {

class DefaultTracingView : public ActionableView {
 public:
  DefaultTracingView() {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal,
        kTrayPopupPaddingHorizontal, 0,
        kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    image_ = new FixedSizedImageView(0, kTrayPopupItemHeight);
    image_->SetImage(
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_TRACING).ToImageSkia());
    AddChildView(image_);

    label_ = new views::Label();
    label_->SetMultiLine(true);
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_->SetText(bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_TRACING));
    AddChildView(label_);
  }

  virtual ~DefaultTracingView() {}

 private:
  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    Shell::GetInstance()->system_tray_delegate()->ShowChromeSlow();
    return true;
  }

  views::ImageView* image_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DefaultTracingView);
};

}  // namespace tray

////////////////////////////////////////////////////////////////////////////////
// ash::TrayTracing

TrayTracing::TrayTracing(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_TRACING),
      default_(NULL) {
  DCHECK(Shell::GetInstance()->delegate());
  DCHECK(system_tray);
  Shell::GetInstance()->system_tray_notifier()->AddTracingObserver(this);
}

TrayTracing::~TrayTracing() {
  Shell::GetInstance()->system_tray_notifier()->RemoveTracingObserver(this);
}

void TrayTracing::SetTrayIconVisible(bool visible) {
  if (tray_view())
    tray_view()->SetVisible(visible);
}

bool TrayTracing::GetInitialVisibility() {
  return false;
}

views::View* TrayTracing::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  if (tray_view() && tray_view()->visible())
    default_ = new tray::DefaultTracingView();
  return default_;
}

views::View* TrayTracing::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayTracing::DestroyDefaultView() {
  default_ = NULL;
}

void TrayTracing::DestroyDetailedView() {
}

void TrayTracing::OnTracingModeChanged(bool value) {
  SetTrayIconVisible(value);
}

}  // namespace ash
