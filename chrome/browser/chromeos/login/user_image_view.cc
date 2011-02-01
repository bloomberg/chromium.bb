// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_view.h"

#include <algorithm>

#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/native_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "views/layout/grid_layout.h"

namespace {

// Margin in pixels from the left and right borders of screen's contents.
const int kHorizontalMargin = 10;
// Margin in pixels from the top and bottom borders of screen's contents.
const int kVerticalMargin = 10;
// Padding between horizontally neighboring elements.
const int kHorizontalPadding = 10;
// Padding between vertically neighboring elements.
const int kVerticalPadding = 10;

// IDs of column sets for grid layout manager.
enum ColumnSets {
  kTitleRow,    // Column set for screen title.
  kImageRow,    // Column set for image from camera and snapshot button.
  kButtonsRow,  // Column set for Skip and OK buttons.
};

}  // namespace

namespace chromeos {

// Image view that can show center throbber above itself or a message at its
// bottom.
class CameraImageView : public views::ImageView {
 public:
  CameraImageView()
      : throbber_(NULL),
        message_(NULL) {}

  ~CameraImageView() {}

  void Init() {
    DCHECK(NULL == throbber_);
    DCHECK(NULL == message_);

    throbber_ = CreateDefaultSmoothedThrobber();
    throbber_->SetVisible(false);
    AddChildView(throbber_);

    message_ = new views::Label();
    message_->SetMultiLine(true);
    message_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    message_->SetVisible(false);
    CorrectLabelFontSize(message_);
    AddChildView(message_);
  }

  void SetInitializingState() {
    ShowThrobber();
    SetMessage(std::wstring());
    SetImage(
        ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_USER_IMAGE_INITIALIZING));
  }

  void SetNormalState() {
    HideThrobber();
    SetMessage(std::wstring());
  }

  void SetErrorState() {
    HideThrobber();
    SetMessage(UTF16ToWide(l10n_util::GetStringUTF16(IDS_USER_IMAGE_NO_VIDEO)));
    SetImage(
        ResourceBundle::GetSharedInstance().GetBitmapNamed(
            IDR_USER_IMAGE_NO_VIDEO));
  }

 private:
  void ShowThrobber() {
    DCHECK(throbber_);
    throbber_->SetVisible(true);
    throbber_->Start();
  }

  void HideThrobber() {
    DCHECK(throbber_);
    throbber_->Stop();
    throbber_->SetVisible(false);
  }

  void SetMessage(const std::wstring& message) {
    DCHECK(message_);
    message_->SetText(message);
    message_->SetVisible(!message.empty());
    Layout();
  }

  // views::View override:
  virtual void Layout() {
    gfx::Size size = GetPreferredSize();
    if (throbber_->IsVisible()) {
      gfx::Size throbber_size = throbber_->GetPreferredSize();
      int throbber_x = (size.width() - throbber_size.width()) / 2;
      int throbber_y = (size.height() - throbber_size.height()) / 2;
      throbber_->SetBounds(throbber_x,
                           throbber_y,
                           throbber_size.width(),
                           throbber_size.height());
    }
    if (message_->IsVisible()) {
      message_->SizeToFit(size.width() - kHorizontalMargin * 2);
      gfx::Size message_size = message_->GetPreferredSize();
      int message_y = size.height() - kVerticalMargin - message_size.height();
      message_->SetBounds(kHorizontalMargin,
                          message_y,
                          message_size.width(),
                          message_size.height());
    }
  }

  // Throbber centered within the view.
  views::Throbber* throbber_;

  // Message, multiline, aligned to the bottom of the view.
  views::Label* message_;

  DISALLOW_COPY_AND_ASSIGN(CameraImageView);
};


using login::kUserImageSize;

UserImageView::UserImageView(Delegate* delegate)
    : title_label_(NULL),
      ok_button_(NULL),
      skip_button_(NULL),
      snapshot_button_(NULL),
      user_image_(NULL),
      delegate_(delegate),
      is_capturing_(true) {
}

UserImageView::~UserImageView() {
}

void UserImageView::Init() {
  // Use rounded rect background.
  set_border(CreateWizardBorder(&BorderDefinition::kScreenBorder));
  views::Painter* painter = CreateWizardPainter(
      &BorderDefinition::kScreenBorder);
  set_background(views::Background::CreateBackgroundPainter(true, painter));

  title_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_USER_IMAGE_SCREEN_TITLE)));
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetMultiLine(true);
  CorrectLabelFontSize(title_label_);

  user_image_ = new CameraImageView();
  user_image_->SetImageSize(
      gfx::Size(kUserImageSize, kUserImageSize));
  user_image_->Init();

  snapshot_button_ = new views::ImageButton(this);
  snapshot_button_->SetFocusable(true);
  snapshot_button_->SetImage(views::CustomButton::BS_NORMAL,
                             ResourceBundle::GetSharedInstance().GetBitmapNamed(
                                 IDR_USER_IMAGE_CAPTURE));
  snapshot_button_->SetImage(views::CustomButton::BS_DISABLED,
                             ResourceBundle::GetSharedInstance().GetBitmapNamed(
                                 IDR_USER_IMAGE_CAPTURE_DISABLED));
  snapshot_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_CHROMEOS_ACC_ACCOUNT_PICTURE));

  ok_button_ = new login::WideButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_OK)));
  ok_button_->SetEnabled(!is_capturing_);

  skip_button_ = new login::WideButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_SKIP)));
  skip_button_->SetEnabled(true);

  InitLayout();
  // Request focus only after the button is added to views hierarchy.
  snapshot_button_->RequestFocus();
  user_image_->SetInitializingState();
}

void UserImageView::InitLayout() {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(GetInsets());
  SetLayoutManager(layout);

  // The title is left-top aligned.
  views::ColumnSet* column_set = layout->AddColumnSet(kTitleRow);
  column_set->AddPaddingColumn(0, kHorizontalMargin);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalMargin);

  // User image and snapshot button are centered horizontally.
  column_set = layout->AddColumnSet(kImageRow);
  column_set->AddPaddingColumn(0, kHorizontalMargin);
  column_set->AddColumn(views::GridLayout::CENTER,
                        views::GridLayout::LEADING,
                        1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalMargin);

  // OK and Skip buttons are in the right bottom corner of the view.
  column_set = layout->AddColumnSet(kButtonsRow);
  column_set->AddPaddingColumn(0, kHorizontalMargin);
  // This column takes the empty space to the left of the buttons.
  column_set->AddPaddingColumn(1, 1);
  column_set->AddColumn(views::GridLayout::TRAILING,
                        views::GridLayout::TRAILING,
                        0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalPadding);
  column_set->AddColumn(views::GridLayout::TRAILING,
                        views::GridLayout::TRAILING,
                        0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kHorizontalMargin);

  // Fill the layout with rows and views now.
  layout->StartRowWithPadding(0, kTitleRow, 0, kVerticalMargin);
  layout->AddView(title_label_);
  layout->StartRowWithPadding(0, kImageRow, 0, kVerticalPadding);
  layout->AddView(user_image_);
  layout->StartRowWithPadding(1, kImageRow, 0, kVerticalPadding);
  layout->AddView(snapshot_button_);
  layout->StartRowWithPadding(0, kButtonsRow, 0, kVerticalPadding);
  layout->AddView(skip_button_);
  layout->AddView(ok_button_);
  layout->AddPaddingRow(0, kVerticalMargin);
}

void UserImageView::UpdateVideoFrame(const SkBitmap& frame) {
  if (!is_capturing_)
    return;

  if (!snapshot_button_->IsEnabled()) {
    user_image_->SetNormalState();
    snapshot_button_->SetEnabled(true);
    snapshot_button_->RequestFocus();
  }
  SkBitmap user_image =
      skia::ImageOperations::Resize(
          frame,
          skia::ImageOperations::RESIZE_BOX,
          kUserImageSize,
          kUserImageSize);

  user_image_->SetImage(&user_image);
}

void UserImageView::ShowCameraInitializing() {
  if (!is_capturing_)
    return;
  snapshot_button_->SetEnabled(false);
  user_image_->SetInitializingState();
}

void UserImageView::ShowCameraError() {
  if (!is_capturing_)
    return;
  snapshot_button_->SetEnabled(false);
  user_image_->SetErrorState();
}

void UserImageView::ViewHierarchyChanged(bool is_add,
                                         views::View* parent,
                                         views::View* child) {
  if (is_add && this == child)
    WizardAccessibilityHelper::GetInstance()->MaybeEnableAccessibility(this);
}

gfx::Size UserImageView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

void UserImageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  DCHECK(delegate_);
  if (sender == snapshot_button_) {
    if (is_capturing_) {
      ok_button_->SetEnabled(true);
      ok_button_->RequestFocus();
      snapshot_button_->SetImage(
          views::CustomButton::BS_NORMAL,
          ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_USER_IMAGE_RECYCLE));
    } else {
      ok_button_->SetEnabled(false);
      snapshot_button_->SetImage(
          views::CustomButton::BS_NORMAL,
          ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_USER_IMAGE_CAPTURE));
    }
    snapshot_button_->SchedulePaint();
    is_capturing_ = !is_capturing_;
  } else if (sender == ok_button_) {
    delegate_->OnOK(user_image_->GetImage());
  } else if (sender == skip_button_) {
    delegate_->OnSkip();
  } else {
    NOTREACHED();
  }
}

}  // namespace chromeos

