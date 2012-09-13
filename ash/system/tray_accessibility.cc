// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_accessibility.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
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
    image->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_ACCESSIBILITY_DARK).
                    ToImageSkia());

    AddChildView(image);
    string16 label = bundle.GetLocalizedString(
        IDS_ASH_STATUS_TRAY_DISABLE_SPOKEN_FEEDBACK);
    AddChildView(new views::Label(label));
    SetAccessibleName(label);
  }

  virtual ~DefaultAccessibilityView() {}

 protected:
  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    if (Shell::GetInstance()->delegate()->IsSpokenFeedbackEnabled())
      Shell::GetInstance()->delegate()->ToggleSpokenFeedback();
    GetWidget()->Close();
    return true;
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(DefaultAccessibilityView);
};

TrayAccessibility::TrayAccessibility()
    : TrayImageItem(IDR_AURA_UBER_TRAY_ACCESSIBILITY),
      default_(NULL),
      detailed_(NULL) {
}

TrayAccessibility::~TrayAccessibility() {}

bool TrayAccessibility::GetInitialVisibility() {
  return Shell::GetInstance()->delegate() &&
      Shell::GetInstance()->delegate()->IsSpokenFeedbackEnabled();
}

views::View* TrayAccessibility::CreateDefaultView(user::LoginStatus status) {
  if (!Shell::GetInstance()->delegate()->IsSpokenFeedbackEnabled())
    return NULL;

  CHECK(default_ == NULL);
  default_ = new DefaultAccessibilityView();

  return default_;
}

views::View* TrayAccessibility::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);
  detailed_ = new views::View;

  detailed_->SetLayoutManager(new
      views::BoxLayout(views::BoxLayout::kHorizontal,
      kTrayPopupPaddingHorizontal, 10, kTrayPopupPaddingBetweenItems));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  views::ImageView* image = new views::ImageView;
  image->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_ACCESSIBILITY_DARK).
      ToImageSkia());

  detailed_->AddChildView(image);
  detailed_->AddChildView(new views::Label(bundle.GetLocalizedString(
      IDS_ASH_STATUS_TRAY_ACCESSIBILITY_TURNED_ON_BUBBLE)));

  return detailed_;
}

void TrayAccessibility::DestroyDefaultView() {
  default_ = NULL;
}

void TrayAccessibility::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayAccessibility::OnAccessibilityModeChanged(bool enabled) {
  if (tray_view())
    tray_view()->SetVisible(enabled);

  if (enabled) {
    PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
  } else if (detailed_) {
    detailed_->GetWidget()->Close();
  }
}

}  // namespace internal
}  // namespace ash
