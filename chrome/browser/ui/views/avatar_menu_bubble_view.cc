// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "views/controls/button/image_button.h"

namespace {

const int kBubbleViewMinWidth = 175;
const int kBubbleViewMaxWidth = 800;
const int kItemHeight = 32;
const int kItemMarginY = 8;
const int kIconWidth = 38;
const int kIconMarginX = 6;
const int kEditProfileButtonMarginX = 8;

inline int Round(double x) {
  return static_cast<int>(x + 0.5);
}

gfx::Rect GetCenteredAndScaledRect(int src_width, int src_height,
                                   int dst_x, int dst_y,
                                   int dst_width, int dst_height) {
  int scaled_width;
  int scaled_height;
  if (src_width > src_height) {
    scaled_width = std::min(src_width, dst_width);
    float scale = static_cast<float>(scaled_width) /
                  static_cast<float>(src_width);
    scaled_height = Round(src_height * scale);
  } else {
    scaled_height = std::min(src_height, dst_height);
    float scale = static_cast<float>(scaled_height) /
                  static_cast<float>(src_height);
    scaled_width = Round(src_width * scale);
  }
  int x = dst_x + (dst_width - scaled_width) / 2;
  int y = dst_y + (dst_height - scaled_height) / 2;
  return gfx::Rect(x, y, scaled_width, scaled_height);
}

class ProfileItemView : public views::CustomButton {
 public:
  ProfileItemView(const AvatarMenuModel::Item& item,
                  views::ButtonListener* listener)
      : views::CustomButton(listener),
        item_(item) {
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    // Draw the profile icon on the left.
    SkBitmap profile_icon = item_.icon;
    gfx::Rect profile_icon_rect = GetCenteredAndScaledRect(
        profile_icon.width(), profile_icon.height(),
        0, 0, kIconWidth, height());
    canvas->DrawBitmapInt(profile_icon, 0, 0, profile_icon.width(),
                          profile_icon.height(), profile_icon_rect.x(),
                          profile_icon_rect.y(), profile_icon_rect.width(),
                          profile_icon_rect.height(), false);

    // If this profile is selected then draw a check mark on the bottom right
    // of the profile icon.
    if (item_.active) {
      SkBitmap check_icon = rb.GetImageNamed(IDR_PROFILE_SELECTED);
      int y = profile_icon_rect.bottom() - check_icon.height();
      int x = profile_icon_rect.right() - check_icon.width() + 2;
      canvas->DrawBitmapInt(check_icon, 0, 0, check_icon.width(),
                            check_icon.height(), x, y, check_icon.width(),
                            check_icon.height(), false);
    }

    // Draw the profile name to the right of the profile icon.
    int name_x = profile_icon_rect.right() + kIconMarginX;
    canvas->DrawStringInt(item_.name, rb.GetFont(ResourceBundle::BaseFont),
                          GetNameColor(), name_x, 0, width() - name_x,
                          height());
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Font font = ResourceBundle::GetSharedInstance().GetFont(
        ResourceBundle::BaseFont);
    int title_width = font.GetStringWidth(item_.name);
    return gfx::Size(kIconWidth + kIconMarginX + title_width, kItemHeight);
  }

 private:
  SkColor GetNameColor() {
    bool normal = state() != views::CustomButton::BS_PUSHED &&
                  state() != views::CustomButton::BS_HOT;
    if (item_.active)
      return normal ? SkColorSetRGB(30, 30, 30) : SkColorSetRGB(0, 0, 0);
    return normal ? SkColorSetRGB(128, 128, 128) : SkColorSetRGB(64, 64, 64);
  }

  AvatarMenuModel::Item item_;
};

} // namespace

class EditProfileButton : public views::ImageButton {
 public:
  EditProfileButton(size_t profile_index, views::ButtonListener* listener)
    : views::ImageButton(listener),
      profile_index_(profile_index) {
  }

  size_t profile_index() {
    return profile_index_;
  }

 private:
  size_t profile_index_;
};

AvatarMenuBubbleView::AvatarMenuBubbleView(Browser* browser)
    : add_profile_link_(NULL),
      browser_(browser),
      edit_profile_button_(NULL) {
  avatar_menu_model_.reset(new AvatarMenuModel(
      &g_browser_process->profile_manager()->GetProfileInfo(),
      this, browser_));
  // Build the menu for the first time.
  OnAvatarMenuModelChanged(avatar_menu_model_.get());
}

AvatarMenuBubbleView::~AvatarMenuBubbleView() {
}

gfx::Size AvatarMenuBubbleView::GetPreferredSize() {
  int max_width = 0;
  int total_height = 0;
  for (size_t i = 0; i < item_views_.size(); ++i) {
    gfx::Size size = item_views_[i]->GetPreferredSize();
    if (i == edit_profile_button_->profile_index()) {
      size.set_width(size.width() +
                     edit_profile_button_->GetPreferredSize().width() +
                     kEditProfileButtonMarginX);
    }

    max_width = std::max(max_width, size.width());
    total_height += size.height() + kItemMarginY;
  }

  gfx::Size add_profile_size = add_profile_link_->GetPreferredSize();
  max_width = std::max(max_width,
                       add_profile_size.width() +  kIconWidth + kIconMarginX);
  total_height += add_profile_link_->GetPreferredSize().height();

  int total_width = std::min(std::max(max_width, kBubbleViewMinWidth),
                             kBubbleViewMaxWidth);
  return gfx::Size(total_width, total_height);
}

void AvatarMenuBubbleView::Layout() {
  int y = 0;
  for (size_t i = 0; i < item_views_.size(); ++i) {
    views::CustomButton* item_view = item_views_[i];
    int item_height = item_view->GetPreferredSize().height();
    int item_width = width();

    if (i == edit_profile_button_->profile_index()) {
      gfx::Size edit_size = edit_profile_button_->GetPreferredSize();
      edit_profile_button_->SetBounds(width() - edit_size.width(), y,
                                      edit_size.width(), item_height);
      item_width -= edit_size.width() + kEditProfileButtonMarginX;
    }

    item_view->SetBounds(0, y, item_width, item_height);
    y += item_height + kItemMarginY;
  }

  add_profile_link_->SetBounds(kIconWidth + kIconMarginX, y, width(),
                               add_profile_link_->GetPreferredSize().height());
}

void AvatarMenuBubbleView::ButtonPressed(views::Button* sender,
                                         const views::Event& event) {
  if (sender == edit_profile_button_) {
    avatar_menu_model_->EditProfile(edit_profile_button_->profile_index());
  } else {
    for (size_t i = 0; i < item_views_.size(); ++i) {
      if (sender == item_views_[i]) {
        avatar_menu_model_->SwitchToProfile(i);
        break;
      }
    }
  }
}

void AvatarMenuBubbleView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(source, add_profile_link_);
  avatar_menu_model_->AddNewProfile();
}

void AvatarMenuBubbleView::BubbleClosing(Bubble* bubble,
                                         bool closed_by_escape) {
}

bool AvatarMenuBubbleView::CloseOnEscape() {
  return true;
}

bool AvatarMenuBubbleView::FadeInOnShow() {
  return false;
}

void AvatarMenuBubbleView::OnAvatarMenuModelChanged(
    AvatarMenuModel* avatar_menu_model) {
  // Unset all our child view references and call RemoveAllChildViews() which
  // will actually delete them.
  add_profile_link_ = NULL;
  edit_profile_button_ = NULL;
  item_views_.clear();
  RemoveAllChildViews(true);

  for (size_t i = 0; i < avatar_menu_model->GetNumberOfItems(); ++i) {
    const AvatarMenuModel::Item& item = avatar_menu_model->GetItemAt(i);
    ProfileItemView* item_view = new ProfileItemView(item, this);
    item_view->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_PROFILES_SWITCH_TO_PROFILE_ACCESSIBLE_NAME, item.name));
    AddChildView(item_view);
    item_views_.push_back(item_view);

    if (item.active) {
      DCHECK(!edit_profile_button_);
      edit_profile_button_ = new EditProfileButton(i, this);
      edit_profile_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_PROFILES_CUSTOMIZE_PROFILE_ACCESSIBLE_NAME, item.name));
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      edit_profile_button_->SetImage(views::CustomButton::BS_NORMAL,
                                     rb.GetImageNamed(IDR_PROFILE_EDIT));
      edit_profile_button_->SetImage(views::CustomButton::BS_HOT,
                                     rb.GetImageNamed(IDR_PROFILE_EDIT_HOVER));
      edit_profile_button_->SetImage(views::CustomButton::BS_PUSHED,
          rb.GetImageNamed(IDR_PROFILE_EDIT_PRESSED));
      edit_profile_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                              views::ImageButton::ALIGN_MIDDLE);
      AddChildView(edit_profile_button_);
    }
  }

  add_profile_link_ = new views::Link(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_PROFILES_CREATE_NEW_PROFILE_LINK)));
  add_profile_link_->set_listener(this);
  add_profile_link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  add_profile_link_->SetNormalColor(SkColorSetRGB(0, 0x79, 0xda));
  AddChildView(add_profile_link_);

  PreferredSizeChanged();
}
