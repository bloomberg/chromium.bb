// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_view.h"

#include <algorithm>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "views/background.h"
#include "views/controls/image_view.h"

namespace {

// Background color of the signout button.
const SkColor kSignoutBackgroundColor = 0xFF007700;

// Horiz/Vert insets for Signout view.
const int kSignoutViewHorizontalInsets = 10;
const int kSignoutViewVerticalInsets = 5;
const int kMinControlHeight = 16;

// 1x Border around image pod.
const SkColor kImageBorderColor = 0xFFCCCCCC;

// Padding between remove button and top right image corner.
const int kRemoveButtonPadding = 3;

}  // namespace

namespace chromeos {

using login::kBackgroundColor;
using login::kTextColor;
using login::kUserImageSize;

// The view that shows the Sign out button below the user's image.
class SignoutView : public views::View {
 public:
  explicit SignoutView(views::LinkListener* link_listener) {
    set_background(
        views::Background::CreateSolidBackground(kSignoutBackgroundColor));

    active_user_label_ = new views::Label(
        l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_ACTIVE_USER));
    const gfx::Font& font =
        ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::SmallFont);
    active_user_label_->SetFont(font);
    active_user_label_->SetBackgroundColor(background()->get_color());
    active_user_label_->SetEnabledColor(kTextColor);

    signout_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_SIGN_OUT));
    signout_link_->set_listener(link_listener);
    signout_link_->SetFont(font);
    signout_link_->set_focusable(true);
    signout_link_->SetBackgroundColor(background()->get_color());
    signout_link_->SetPressedColor(kTextColor);
    signout_link_->SetDisabledColor(kTextColor);
    signout_link_->SetEnabledColor(kTextColor);
    signout_link_->set_id(VIEW_ID_SCREEN_LOCKER_SIGNOUT_LINK);

    AddChildView(active_user_label_);
    AddChildView(signout_link_);
  }

  // views::View overrides.
  virtual void Layout() {
    gfx::Size label = active_user_label_->GetPreferredSize();
    gfx::Size button = signout_link_->GetPreferredSize();
    active_user_label_->SetBounds(
        kSignoutViewHorizontalInsets, (height() - label.height()) / 2,
        label.width(), label.height());
    signout_link_->SetBounds(
        width() - button.width() - kSignoutViewHorizontalInsets,
        (height() - button.height()) / 2,
        button.width(), button.height());
  }

  virtual gfx::Size GetPreferredSize() {
    gfx::Size label = active_user_label_->GetPreferredSize();
    gfx::Size button = signout_link_->GetPreferredSize();
    int width =
      label.width() + button.width() + 2 * kSignoutViewHorizontalInsets;
    int height =
      std::max(kMinControlHeight, std::max(label.height(), button.height())) +
      kSignoutViewVerticalInsets * 2;
    return gfx::Size(width, height);
  }

  views::Link* signout_link() { return signout_link_; }

 private:
  views::Label* active_user_label_;
  views::Link* signout_link_;

  DISALLOW_COPY_AND_ASSIGN(SignoutView);
};

class RemoveButton : public views::TextButton {
 public:
  RemoveButton(views::ButtonListener* listener,
               const SkBitmap& icon,
               const string16& text,
               const gfx::Point& top_right)
    : views::TextButton(listener, string16()),
      icon_(icon),
      text_(text),
      top_right_(top_right),
      was_first_click_(false) {
    SetEnabledColor(SK_ColorWHITE);
    SetDisabledColor(SK_ColorWHITE);
    SetHighlightColor(SK_ColorWHITE);
    SetHoverColor(SK_ColorWHITE);
    SetIcon(icon_);
    UpdatePosition();
  }

 protected:
  // Overridden from View:
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    SetIcon(icon_);
    views::TextButton::SetText(string16());
    ClearMaxTextSize();
    set_background(NULL);
    set_border(new views::TextButtonBorder);
    UpdatePosition();
    views::TextButton::OnMouseExited(event);
    was_first_click_ = false;
  }

  virtual void NotifyClick(const views::Event& event) OVERRIDE {
    if (!was_first_click_) {
      // On first click transform image to "remove" label.
      SetIcon(SkBitmap());
      views::TextButton::SetText(text_);

      const SkColor kStrokeColor = SK_ColorWHITE;
      const SkColor kButtonColor = 0xFFE94949;
      const int kStrokeWidth = 1;
      const int kVerticalPadding = 4;
      const int kHorizontalPadding = 8;
      const int kCornerRadius = 4;

      set_background(CreateRoundedBackground(kCornerRadius, kStrokeWidth,
                                             kButtonColor, kStrokeColor));

      set_border(views::Border::CreateEmptyBorder(kVerticalPadding,
          kHorizontalPadding, kVerticalPadding, kHorizontalPadding));

      UpdatePosition();
      was_first_click_ = true;
    } else {
      // On second click propagate to base class to fire ButtonPressed.
      views::TextButton::NotifyClick(event);
    }
  }

  virtual void SetText(const string16& text) OVERRIDE {
    text_ = text;
  }

 private:
  // Update button position and schedule paint event for the view and parent.
  void UpdatePosition() {
    gfx::Size size = GetPreferredSize();
    gfx::Point origin = top_right_;
    origin.Offset(-size.width(), 0);
    SetBoundsRect(gfx::Rect(origin, size));

    if (parent())
      parent()->SchedulePaint();
  }

  SkBitmap icon_;
  string16 text_;
  gfx::Point top_right_;
  bool was_first_click_;

  DISALLOW_COPY_AND_ASSIGN(RemoveButton);
};

class PodImageView : public views::ImageView {
 public:
  explicit PodImageView(const UserView::Delegate* delegate)
      : delegate_(delegate) { }

  void SetImage(const SkBitmap& image, const SkBitmap& image_hot) {
    image_ = image;
    image_hot_ = image_hot;
    views::ImageView::SetImage(image_);
  }

 protected:
  // Overridden from View:
  gfx::NativeCursor GetCursor(const views::MouseEvent& event) OVERRIDE {
    return delegate_->IsUserSelected() ? NULL : gfx::GetCursor(GDK_HAND2);
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    views::ImageView::SetImage(image_hot_);
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    views::ImageView::SetImage(image_);
  }

 private:
  const UserView::Delegate* delegate_;

  SkBitmap image_;
  SkBitmap image_hot_;

  DISALLOW_COPY_AND_ASSIGN(PodImageView);
};

UserView::UserView(Delegate* delegate, bool is_login, bool need_background)
    : delegate_(delegate),
      signout_view_(NULL),
      ignore_signout_click_(false),
      image_view_(NULL),
      remove_button_(NULL) {
  DCHECK(delegate);
  if (!is_login)
    signout_view_ = new SignoutView(this);

  image_view_ = new PodImageView(delegate);
  image_view_->set_border(
      views::Border::CreateSolidBorder(1, kImageBorderColor));

  Init(need_background);
}

void UserView::Init(bool need_background) {
  if (need_background) {
    image_view_->set_background(
        views::Background::CreateSolidBackground(kBackgroundColor));
  }

  // UserView's layout never changes, so let's layout once here.
  image_view_->SetBounds(0, 0, kUserImageSize, kUserImageSize);
  AddChildView(image_view_);

  if (signout_view_) {
    signout_view_->SetBounds(0, kUserImageSize, kUserImageSize,
                             signout_view_->GetPreferredSize().height());
    AddChildView(signout_view_);
  }

  remove_button_ = new RemoveButton(
      this,
      *ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_CLOSE_BAR_H),
      l10n_util::GetStringUTF16(IDS_LOGIN_REMOVE),
      gfx::Point(kUserImageSize - kRemoveButtonPadding, kRemoveButtonPadding));
  remove_button_->SetVisible(false);
  AddChildView(remove_button_);
}

void UserView::SetImage(const SkBitmap& image, const SkBitmap& image_hot) {
  int desired_size = std::min(image.width(), image.height());
  // Desired size is not preserved if it's greater than 75% of kUserImageSize.
  if (desired_size * 4 > 3 * kUserImageSize)
    desired_size = kUserImageSize;
  image_view_->SetImageSize(gfx::Size(desired_size, desired_size));
  image_view_->SetImage(image, image_hot);
}

void UserView::SetTooltipText(const string16& text) {
  DCHECK(image_view_);
  image_view_->SetTooltipText(text);
}

gfx::Size UserView::GetPreferredSize() {
  return gfx::Size(
      kUserImageSize,
      kUserImageSize +
      (signout_view_ ? signout_view_->GetPreferredSize().height() : 0));
}

void UserView::SetSignoutEnabled(bool enabled) {
  ignore_signout_click_ = !enabled;
}

void UserView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK(delegate_);
  DCHECK(signout_view_);
  if (!ignore_signout_click_ && signout_view_->signout_link() == source)
    delegate_->OnSignout();
}

void UserView::SetRemoveButtonVisible(bool flag) {
  remove_button_->SetVisible(flag);
}

void UserView::ButtonPressed(views::Button* sender, const views::Event& event) {
  DCHECK(delegate_);
  if (remove_button_ == sender)
    delegate_->OnRemoveUser();
}

void UserView::OnLocaleChanged() {
  remove_button_->SetText(l10n_util::GetStringUTF16(IDS_LOGIN_REMOVE));
  delegate_->OnLocaleChanged();
}

}  // namespace chromeos
