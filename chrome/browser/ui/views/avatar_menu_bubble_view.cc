// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"

#include <algorithm>

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"

namespace {

const int kItemHeight = 44;
const int kItemMarginY = 4;
const int kIconMarginX = 6;
const int kSeparatorPaddingY = 5;
const int kMaxItemTextWidth = 200;

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


// HighlightDelegate ----------------------------------------------------------

// Delegate to callback when the highlight state of a control changes.
class HighlightDelegate {
 public:
  virtual ~HighlightDelegate() {}
  virtual void OnHighlightStateChanged() = 0;
  virtual void OnFocusStateChanged(bool has_focus) = 0;
};


// EditProfileLink ------------------------------------------------------------

// A custom Link control that forwards highlight state changes. We need to do
// this to make sure that the ProfileItemView looks highlighted even when
// the mouse is over this link.
class EditProfileLink : public views::Link {
 public:
  explicit EditProfileLink(const string16& title,
                           HighlightDelegate* delegate);

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  views::CustomButton::ButtonState state() { return state_; }

 private:
  HighlightDelegate* delegate_;
  views::CustomButton::ButtonState state_;
};

EditProfileLink::EditProfileLink(const string16& title,
                                 HighlightDelegate* delegate)
    : views::Link(title),
      delegate_(delegate),
      state_(views::CustomButton::BS_NORMAL) {
}

void EditProfileLink::OnMouseEntered(const views::MouseEvent& event) {
  views::Link::OnMouseEntered(event);
  state_ = views::CustomButton::BS_HOT;
  delegate_->OnHighlightStateChanged();
}

void EditProfileLink::OnMouseExited(const views::MouseEvent& event) {
  views::Link::OnMouseExited(event);
  state_ = views::CustomButton::BS_NORMAL;
  delegate_->OnHighlightStateChanged();
}

void EditProfileLink::OnFocus() {
  views::Link::OnFocus();
  delegate_->OnFocusStateChanged(true);
}

void EditProfileLink::OnBlur() {
  views::Link::OnBlur();
  state_ = views::CustomButton::BS_NORMAL;
  delegate_->OnFocusStateChanged(false);
}


// ProfileImageView -----------------------------------------------------------

// A custom image view that ignores mouse events so that the parent can receive
// them instead.
class ProfileImageView : public views::ImageView {
 public:
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE;
};

bool ProfileImageView::HitTest(const gfx::Point& l) const {
  return false;
}


// ProfileItemView ------------------------------------------------------------

// Control that shows information about a single profile.
class ProfileItemView : public views::CustomButton,
                        public HighlightDelegate {
 public:
  ProfileItemView(const AvatarMenuModel::Item& item,
                  views::ButtonListener* switch_profile_listener,
                  views::LinkListener* edit_profile_listener);

  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  virtual void OnHighlightStateChanged() OVERRIDE;
  virtual void OnFocusStateChanged(bool has_focus) OVERRIDE;

  EditProfileLink* edit_link() { return edit_link_; }
  const AvatarMenuModel::Item& item() { return item_; }

 private:
  static SkBitmap GetBadgedIcon(const SkBitmap& icon);

  bool IsHighlighted();

  EditProfileLink* edit_link_;
  views::ImageView* image_view_;
  AvatarMenuModel::Item item_;
  views::Label* name_label_;
  views::Label* sync_state_label_;
};

ProfileItemView::ProfileItemView(const AvatarMenuModel::Item& item,
                                 views::ButtonListener* switch_profile_listener,
                                 views::LinkListener* edit_profile_listener)
    : views::CustomButton(switch_profile_listener),
      item_(item) {
  image_view_ = new ProfileImageView();
  SkBitmap profile_icon = *item_.icon.ToSkBitmap();
  if (item_.active) {
    SkBitmap badged_icon(GetBadgedIcon(profile_icon));
    image_view_->SetImage(badged_icon);
  } else {
    image_view_->SetImage(profile_icon);
  }
  AddChildView(image_view_);

  // Add a label to show the profile name.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::Font base_font = rb.GetFont(ui::ResourceBundle::BaseFont);
  const int style = item_.active ? gfx::Font::BOLD : 0;
  const int kNameFontDelta = 1;
  name_label_ = new views::Label(item_.name,
                                 base_font.DeriveFont(kNameFontDelta, style));
  name_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(name_label_);

  // Add a label to show the sync state.
  const int kStateFontDelta = -1;
  sync_state_label_ = new views::Label();
  if (item_.signed_in)
    sync_state_label_->SetEmail(item.sync_state);
  else
    sync_state_label_->SetText(item_.sync_state);
  sync_state_label_->SetFont(base_font.DeriveFont(kStateFontDelta));
  sync_state_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  sync_state_label_->SetEnabled(false);
  AddChildView(sync_state_label_);

  // Add an edit profile link.
  edit_link_ = new EditProfileLink(
      l10n_util::GetStringUTF16(IDS_PROFILES_EDIT_PROFILE_LINK), this);
  edit_link_->set_listener(edit_profile_listener);
  edit_link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  edit_link_->SetEnabledColor(SkColorSetRGB(0xe3, 0xed, 0xf6));
  edit_link_->SetHasFocusBorder(true);
  AddChildView(edit_link_);

  OnHighlightStateChanged();
}

gfx::Size ProfileItemView::GetPreferredSize() {
  int text_width = std::max(name_label_->GetPreferredSize().width(),
                            sync_state_label_->GetPreferredSize().width());
  text_width = std::max(edit_link_->GetPreferredSize().width(), text_width);
  text_width = std::min(kMaxItemTextWidth, text_width);
  return gfx::Size(profiles::kAvatarIconWidth + kIconMarginX + text_width,
                   kItemHeight);
}

void ProfileItemView::Layout() {
  // Profile icon.
  gfx::Rect icon_rect;
  if (item_.active) {
    // If this is the active item then the icon is already scaled and so
    // just use the preferred size.
    icon_rect.set_size(image_view_->GetPreferredSize());
    icon_rect.set_y((height() - icon_rect.height()) / 2);
  } else {
    const SkBitmap& icon = image_view_->GetImage();
    icon_rect = GetCenteredAndScaledRect(icon.width(), icon.height(), 0, 0,
        profiles::kAvatarIconWidth, height());
  }
  image_view_->SetBoundsRect(icon_rect);

  int label_x = profiles::kAvatarIconWidth + kIconMarginX;
  int max_label_width = std::max(width() - label_x, 0);
  gfx::Size name_size = name_label_->GetPreferredSize();
  name_size.set_width(std::min(name_size.width(), max_label_width));
  gfx::Size state_size = sync_state_label_->GetPreferredSize();
  state_size.set_width(std::min(state_size.width(), max_label_width));
  gfx::Size edit_size = edit_link_->GetPreferredSize();
  edit_size.set_width(std::min(edit_size.width(), max_label_width));

  const int kNameStatePaddingY = 2;
  int labels_height = name_size.height() + kNameStatePaddingY +
      std::max(state_size.height(), edit_size.height());
  int y = (height() - labels_height) / 2;
  name_label_->SetBounds(label_x, y, name_size.width(), name_size.height());

  int bottom = y + labels_height;
  sync_state_label_->SetBounds(label_x, bottom - state_size.height(),
                               state_size.width(), state_size.height());
  // The edit link overlaps the sync state label.
  edit_link_->SetBounds(label_x, bottom - edit_size.height(),
                        edit_size.width(), edit_size.height());
}

void ProfileItemView::OnMouseEntered(const views::MouseEvent& event) {
  views::CustomButton::OnMouseEntered(event);
  OnHighlightStateChanged();
}

void ProfileItemView::OnMouseExited(const views::MouseEvent& event) {
  views::CustomButton::OnMouseExited(event);
  OnHighlightStateChanged();
}

void ProfileItemView::OnFocus() {
  views::CustomButton::OnFocus();
  OnFocusStateChanged(true);
}

void ProfileItemView::OnBlur() {
  views::CustomButton::OnBlur();
  OnFocusStateChanged(false);
}

void ProfileItemView::OnHighlightStateChanged() {
  set_background(IsHighlighted() ? views::Background::CreateSolidBackground(
      SkColorSetRGB(0xe3, 0xed, 0xf6)) : NULL);
  SkColor background_color = background() ?
      background()->get_color() : views::BubbleDelegateView::kBackgroundColor;
  name_label_->SetBackgroundColor(background_color);
  sync_state_label_->SetBackgroundColor(background_color);
  edit_link_->SetBackgroundColor(background_color);

  bool show_edit = IsHighlighted() && item_.active;
  sync_state_label_->SetVisible(!show_edit);
  edit_link_->SetVisible(show_edit);
  SchedulePaint();
}

void ProfileItemView::OnFocusStateChanged(bool has_focus) {
  if (!has_focus && state() != views::CustomButton::BS_DISABLED)
    SetState(views::CustomButton::BS_NORMAL);
  OnHighlightStateChanged();
}

// static
SkBitmap ProfileItemView::GetBadgedIcon(const SkBitmap& icon) {
  gfx::Rect icon_rect = GetCenteredAndScaledRect(icon.width(), icon.height(),
      0, 0, profiles::kAvatarIconWidth, kItemHeight);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const SkBitmap* badge = rb.GetImageNamed(IDR_PROFILE_SELECTED).ToSkBitmap();
  const float kBadgeOverlapRatioX = 1.0f / 5.0f;
  int width = icon_rect.width() + badge->width() * kBadgeOverlapRatioX;
  const float kBadgeOverlapRatioY = 1.0f / 3.0f;
  int height = icon_rect.height() + badge->height() * kBadgeOverlapRatioY;

  gfx::Canvas canvas(gfx::Size(width, height), false);
  canvas.DrawBitmapInt(icon, 0, 0, icon.width(), icon.height(), 0, 0,
                       icon_rect.width(), icon_rect.height(), true);
  canvas.DrawBitmapInt(*badge, width - badge->width(),
                       height - badge->height());
  return canvas.ExtractBitmap();
}

bool ProfileItemView::IsHighlighted() {
  return state() == views::CustomButton::BS_PUSHED ||
         state() == views::CustomButton::BS_HOT ||
         edit_link_->state() == views::CustomButton::BS_PUSHED ||
         edit_link_->state() == views::CustomButton::BS_HOT ||
         HasFocus() ||
         edit_link_->HasFocus();
}

}  // namespace


// AvatarMenuBubbleView -------------------------------------------------------

AvatarMenuBubbleView::AvatarMenuBubbleView(
    views::View* anchor_view,
    views::BubbleBorder::ArrowLocation arrow_location,
    const gfx::Rect& anchor_rect,
    Browser* browser)
    : BubbleDelegateView(anchor_view, arrow_location),
      add_profile_link_(NULL),
      anchor_rect_(anchor_rect),
      browser_(browser) {
  avatar_menu_model_.reset(new AvatarMenuModel(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      this, browser_));
}

AvatarMenuBubbleView::~AvatarMenuBubbleView() {
}

gfx::Size AvatarMenuBubbleView::GetPreferredSize() {
  if (!add_profile_link_)
    return gfx::Size();

  int max_width = 0;
  int total_height = 0;
  for (size_t i = 0; i < item_views_.size(); ++i) {
    gfx::Size size = item_views_[i]->GetPreferredSize();
    max_width = std::max(max_width, size.width());
    total_height += size.height() + kItemMarginY;
  }

  total_height += kSeparatorPaddingY * 2 +
                  separator_->GetPreferredSize().height();

  gfx::Size add_profile_size = add_profile_link_->GetPreferredSize();
  max_width = std::max(max_width,
      add_profile_size.width() + profiles::kAvatarIconWidth + kIconMarginX);
  total_height += add_profile_link_->GetPreferredSize().height();

  const int kBubbleViewMaxWidth = 800;
  const int kBubbleViewMinWidth = 175;
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
    item_view->SetBounds(0, y, item_width, item_height);
    y += item_height + kItemMarginY;
  }

  y += kSeparatorPaddingY;
  int separator_height = separator_->GetPreferredSize().height();
  separator_->SetBounds(0, y, width(), separator_height);
  y += kSeparatorPaddingY + separator_height;

  add_profile_link_->SetBounds(profiles::kAvatarIconWidth + kIconMarginX, y,
      width(), add_profile_link_->GetPreferredSize().height());
}

bool AvatarMenuBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() != ui::VKEY_DOWN &&
      accelerator.key_code() != ui::VKEY_UP)
    return BubbleDelegateView::AcceleratorPressed(accelerator);

  if (item_views_.empty())
    return true;

  // Find the currently focused item. Note that if there is no focused item, the
  // code below correctly handles a |focus_index| of -1.
  int focus_index = -1;
  for (size_t i = 0; i < item_views_.size(); ++i) {
    if (item_views_[i]->HasFocus()) {
      focus_index = i;
      break;
    }
  }

  // Moved the focus up or down by 1.
  if (accelerator.key_code() == ui::VKEY_DOWN)
    focus_index = (focus_index + 1) % item_views_.size();
  else
    focus_index = ((focus_index > 0) ? focus_index : item_views_.size()) - 1;
  item_views_[focus_index]->RequestFocus();

  return true;
}

void AvatarMenuBubbleView::ButtonPressed(views::Button* sender,
                                         const views::Event& event) {
  for (size_t i = 0; i < item_views_.size(); ++i) {
    ProfileItemView* item_view = static_cast<ProfileItemView*>(item_views_[i]);
    if (sender == item_view) {
      // Clicking on the active profile shouldn't do anything.
      if (!item_view->item().active) {
        avatar_menu_model_->SwitchToProfile(
            i, browser::DispositionFromEventFlags(event.flags()) == NEW_WINDOW);
      }
      break;
    }
  }
}

void AvatarMenuBubbleView::LinkClicked(views::Link* source, int event_flags) {
  if (source == add_profile_link_) {
    avatar_menu_model_->AddNewProfile();
    return;
  }

  for (size_t i = 0; i < item_views_.size(); ++i) {
    ProfileItemView* item_view = static_cast<ProfileItemView*>(item_views_[i]);
    if (source == item_view->edit_link()) {
      avatar_menu_model_->EditProfile(i);
      return;
    }
  }
}

gfx::Rect AvatarMenuBubbleView::GetAnchorRect() {
  return anchor_rect_;
}

void AvatarMenuBubbleView::Init() {
  // Build the menu for the first time.
  OnAvatarMenuModelChanged(avatar_menu_model_.get());
  AddAccelerator(ui::Accelerator(ui::VKEY_DOWN, 0));
  AddAccelerator(ui::Accelerator(ui::VKEY_UP, 0));
}

void AvatarMenuBubbleView::OnAvatarMenuModelChanged(
    AvatarMenuModel* avatar_menu_model) {
  // Unset all our child view references and call RemoveAllChildViews() which
  // will actually delete them.
  add_profile_link_ = NULL;
  item_views_.clear();
  RemoveAllChildViews(true);

  for (size_t i = 0; i < avatar_menu_model->GetNumberOfItems(); ++i) {
    const AvatarMenuModel::Item& item = avatar_menu_model->GetItemAt(i);
    ProfileItemView* item_view = new ProfileItemView(item, this, this);
    item_view->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_PROFILES_SWITCH_TO_PROFILE_ACCESSIBLE_NAME, item.name));
    item_view->set_focusable(true);
    AddChildView(item_view);
    item_views_.push_back(item_view);
  }

  separator_ = new views::Separator();
  AddChildView(separator_);

  add_profile_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_PROFILES_CREATE_NEW_PROFILE_LINK));
  add_profile_link_->set_listener(this);
  add_profile_link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  add_profile_link_->SetBackgroundColor(color());
  add_profile_link_->SetEnabledColor(SkColorSetRGB(0xe3, 0xed, 0xf6));
  AddChildView(add_profile_link_);

  // If the bubble has already been shown then resize and reposition the bubble.
  Layout();
  if (GetBubbleFrameView())
    SizeToContents();
}
