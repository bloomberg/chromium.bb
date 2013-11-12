// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/drive/tray_drive_notice.h"

#include "ash/shell.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace internal {

namespace {

// Vertical spacing between notice label and the detailed view container.
const int kNoticeLabelVerticalSpacing = 3;

// Bottom spacing inside border for notice label in detailed view.
const int kNoticeLabelBorderBottomSpacing = 5;

// The time to show the tray item notice.
const int kTimeVisibleSeconds = 30;

}  // namespace

class DriveNoticeDefaultView : public TrayItemMore {
 public:
  DriveNoticeDefaultView(SystemTrayItem* owner) :
      TrayItemMore(owner, true) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DRIVE).ToImageSkia());
    SetLabel(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DRIVE_OFFLINE_NOTICE));
  }

  virtual ~DriveNoticeDefaultView() {}
};

class DriveNoticeDetailedView : public TrayDetailsView,
                                public ViewClickListener {
 public:
  DriveNoticeDetailedView(SystemTrayItem* owner)
      : TrayDetailsView(owner),
        settings_button_(NULL) {
    Reset();

    CreateScrollableList();
    CreateNoticeLabel();
    CreateSettingsButton();
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_DRIVE_OFFLINE_FOOTER, this);

    Layout();
  }

  virtual ~DriveNoticeDetailedView() {}

 private:
  void CreateNoticeLabel() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    views::Label* notice_label = new views::Label(
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_DRIVE_DISABLE_OFFLINE));
    notice_label->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal,
      0,
      kNoticeLabelVerticalSpacing,
      kTrayPopupPaddingBetweenItems));
    notice_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    notice_label->SetMultiLine(true);
    notice_label->SetFont(
        notice_label->font().DeriveFont(0, gfx::Font::NORMAL));

    int margin = kTrayPopupPaddingHorizontal +
        kTrayPopupDetailsLabelExtraLeftMargin;
    int left_margin = base::i18n::IsRTL() ? 0 : margin;
    int right_margin = base::i18n::IsRTL() ? margin : 0;
    notice_label->set_border(
        views::Border::CreateEmptyBorder(
            kTrayPopupPaddingBetweenItems,
            left_margin,
            kNoticeLabelBorderBottomSpacing,
            right_margin));

    scroll_content()->AddChildView(notice_label);
  }

  void CreateSettingsButton() {
    HoverHighlightView* redirect_button = new HoverHighlightView(this);
    redirect_button->AddLabel(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DRIVE_SETTINGS),
        gfx::Font::NORMAL);
    AddChildView(redirect_button);
    settings_button_ = redirect_button;
  }

  // Overridden from ViewClickListener.
  virtual void OnViewClicked(views::View* sender) OVERRIDE {
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    if (sender == footer()->content()) {
      owner()->system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
    } else if (sender == settings_button_) {
      delegate->ShowDriveSettings();
    }
  }

  views::View* settings_button_;

  DISALLOW_COPY_AND_ASSIGN(DriveNoticeDetailedView);
};

TrayDriveNotice::TrayDriveNotice(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_DRIVE_LIGHT),
      default_view_(NULL),
      detailed_view_(NULL),
      showing_item_(false),
      time_visible_secs_(kTimeVisibleSeconds) {
  Shell::GetInstance()->system_tray_notifier()->AddDriveObserver(this);
}

TrayDriveNotice::~TrayDriveNotice() {
  Shell::GetInstance()->system_tray_notifier()->RemoveDriveObserver(this);
}

views::View* TrayDriveNotice::GetTrayView() {
  return TrayImageItem::tray_view();
}

void TrayDriveNotice::SetTimeVisibleForTest(int time_visible_secs) {
  time_visible_secs_ = time_visible_secs;
}

bool TrayDriveNotice::GetInitialVisibility() {
  return false;
}

views::View* TrayDriveNotice::CreateDefaultView(user::LoginStatus status) {
  DCHECK(status == user::LOGGED_IN_USER);
  CHECK(default_view_ == NULL);

  if (!showing_item_)
    return NULL;

  default_view_ = new DriveNoticeDefaultView(this);
  return default_view_;
}

views::View* TrayDriveNotice::CreateDetailedView(user::LoginStatus status) {
  if (!showing_item_)
    return NULL;

  DCHECK(status == user::LOGGED_IN_USER);
  detailed_view_ = new DriveNoticeDetailedView(this);
  return detailed_view_;
}

void TrayDriveNotice::DestroyDefaultView() {
  default_view_ = NULL;
}

void TrayDriveNotice::DestroyDetailedView() {
  detailed_view_ = NULL;
}

void TrayDriveNotice::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayDriveNotice::OnDriveJobUpdated(const DriveOperationStatus& status) {
}

void TrayDriveNotice::OnDriveOfflineEnabled() {
  showing_item_ = true;
  tray_view()->SetVisible(true);
  visibility_timer_.Start(FROM_HERE,
                          base::TimeDelta::FromSeconds(time_visible_secs_),
                          this,
                          &TrayDriveNotice::HideNotice);
}

void TrayDriveNotice::HideNotice() {
  showing_item_ = false;
  tray_view()->SetVisible(false);
}

}  // namespace internal
}  // namespace ash
