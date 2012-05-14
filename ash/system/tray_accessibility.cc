// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_accessibility.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

class DefaultAccessibilityView : public ActionableView {
 public:
  DefaultAccessibilityView() {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                          kTrayPopupPaddingHorizontal,
                                          0,
                                          kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    FixedSizedImageView* image =
        new FixedSizedImageView(0, kTrayPopupItemHeight);
    image->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_ACCESSIBILITY).
                    ToSkBitmap());

    AddChildView(image);
    string16 label = bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_DISABLE_SPOKEN_FEEDBACK);
    AddChildView(new views::Label(label));
    SetAccessibleName(label);
  }

  virtual ~DefaultAccessibilityView() {}

 protected:
  // Overridden from ActionableView.
  virtual bool PerformAction(const views::Event& event) OVERRIDE {
    ash::Shell::GetInstance()->tray_delegate()->SetEnableSpokenFeedback(false);
    GetWidget()->Close();
    return true;
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(DefaultAccessibilityView);
};

TrayAccessibility::TrayAccessibility()
    : TrayImageItem(IDR_AURA_UBER_TRAY_ACCESSIBILITY),
      default_(NULL),
      detailed_(NULL),
      string_id_(0) {
}

TrayAccessibility::~TrayAccessibility() {}

bool TrayAccessibility::GetInitialVisibility() {
  return ash::Shell::GetInstance()->tray_delegate()->IsInAccessibilityMode();
}

views::View* TrayAccessibility::CreateDefaultView(user::LoginStatus status) {
  if (!ash::Shell::GetInstance()->tray_delegate()->IsInAccessibilityMode())
    return NULL;

  DCHECK(string_id_);
  CHECK(default_ == NULL);
  default_ = new DefaultAccessibilityView();

  return default_;
}

views::View* TrayAccessibility::CreateDetailedView(user::LoginStatus status) {
  DCHECK(string_id_);
  CHECK(detailed_ == NULL);
  detailed_ = new views::View;

  detailed_->SetLayoutManager(new
      views::BoxLayout(views::BoxLayout::kHorizontal,
      kTrayPopupPaddingHorizontal, 10, kTrayPopupPaddingBetweenItems));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  views::ImageView* image = new views::ImageView;
  image->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_ACCESSIBILITY).
      ToSkBitmap());

  detailed_->AddChildView(image);
  detailed_->AddChildView(new views::Label(
        bundle.GetLocalizedString(string_id_)));

  return detailed_;
}

void TrayAccessibility::DestroyDefaultView() {
  default_ = NULL;
}

void TrayAccessibility::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayAccessibility::OnAccessibilityModeChanged(bool enabled,
                                                   int string_id) {
  if (tray_view())
    tray_view()->SetVisible(enabled);

  if (enabled) {
    string_id_ = string_id;
    PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
  } else if (detailed_) {
    string_id_ = 0;
    detailed_->GetWidget()->Close();
  }
}

}  // namespace internal
}  // namespace ash
