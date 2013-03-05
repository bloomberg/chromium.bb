// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/avatar_menu_bubble_view.h"

#include <algorithm>

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/widget/widget.h"

namespace {

const int kItemHeight = 44;
const int kItemMarginY = 4;
const int kIconMarginX = 6;
const int kSeparatorPaddingY = 5;
const int kMaxItemTextWidth = 200;
const SkColor kHighlightColor = 0xFFE3EDF6;

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

// BadgeImageSource -----------------------------------------------------------
class BadgeImageSource: public gfx::CanvasImageSource {
 public:
  BadgeImageSource(const gfx::ImageSkia& icon,
                   const gfx::Size& icon_size,
                   const gfx::ImageSkia& badge);

  virtual ~BadgeImageSource();

  // Overridden from CanvasImageSource:
  virtual void Draw(gfx::Canvas* canvas) OVERRIDE;

 private:
  gfx::Size ComputeSize(const gfx::ImageSkia& icon,
                        const gfx::Size& size,
                        const gfx::ImageSkia& badge);

  const gfx::ImageSkia icon_;
  gfx::Size icon_size_;
  const gfx::ImageSkia badge_;

  DISALLOW_COPY_AND_ASSIGN(BadgeImageSource);
};

BadgeImageSource::BadgeImageSource(const gfx::ImageSkia& icon,
                                   const gfx::Size& icon_size,
                                   const gfx::ImageSkia& badge)
    : gfx::CanvasImageSource(ComputeSize(icon, icon_size, badge), false),
      icon_(icon),
      icon_size_(icon_size),
      badge_(badge) {
}

BadgeImageSource::~BadgeImageSource() {
}

void BadgeImageSource::Draw(gfx::Canvas* canvas) {
  canvas->DrawImageInt(icon_, 0, 0, icon_.width(), icon_.height(), 0, 0,
                       icon_size_.width(), icon_size_.height(), true);
  canvas->DrawImageInt(badge_, size().width() - badge_.width(),
                       size().height() - badge_.height());
}

gfx::Size BadgeImageSource::ComputeSize(const gfx::ImageSkia& icon,
                                        const gfx::Size& icon_size,
                                        const gfx::ImageSkia& badge) {
  const float kBadgeOverlapRatioX = 1.0f / 5.0f;
  int width = icon_size.width() + badge.width() * kBadgeOverlapRatioX;
  const float kBadgeOverlapRatioY = 1.0f / 3.0f;
  int height = icon_size.height() + badge.height() * kBadgeOverlapRatioY;
  return gfx::Size(width, height);
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

  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
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
      state_(views::CustomButton::STATE_NORMAL) {
}

void EditProfileLink::OnMouseEntered(const ui::MouseEvent& event) {
  views::Link::OnMouseEntered(event);
  state_ = views::CustomButton::STATE_HOVERED;
  delegate_->OnHighlightStateChanged();
}

void EditProfileLink::OnMouseExited(const ui::MouseEvent& event) {
  views::Link::OnMouseExited(event);
  state_ = views::CustomButton::STATE_NORMAL;
  delegate_->OnHighlightStateChanged();
}

void EditProfileLink::OnFocus() {
  views::Link::OnFocus();
  delegate_->OnFocusStateChanged(true);
}

void EditProfileLink::OnBlur() {
  views::Link::OnBlur();
  state_ = views::CustomButton::STATE_NORMAL;
  delegate_->OnFocusStateChanged(false);
}


// ProfileImageView -----------------------------------------------------------

// A custom image view that ignores mouse events so that the parent can receive
// them instead.
class ProfileImageView : public views::ImageView {
 public:
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE;
};

bool ProfileImageView::HitTestRect(const gfx::Rect& rect) const {
  return false;
}


// ProfileItemView ------------------------------------------------------------

// Control that shows information about a single profile.
class ProfileItemView : public views::CustomButton,
                        public HighlightDelegate {
 public:
  ProfileItemView(const AvatarMenuModel::Item& item,
                  AvatarMenuBubbleView* parent);

  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  virtual void OnHighlightStateChanged() OVERRIDE;
  virtual void OnFocusStateChanged(bool has_focus) OVERRIDE;

  const AvatarMenuModel::Item& item() const { return item_; }
  EditProfileLink* edit_link() { return edit_link_; }

 private:
  static gfx::ImageSkia GetBadgedIcon(const gfx::ImageSkia& icon);

  bool IsHighlighted();

  AvatarMenuModel::Item item_;
  AvatarMenuBubbleView* parent_;
  views::ImageView* image_view_;
  views::Label* name_label_;
  views::Label* sync_state_label_;
  EditProfileLink* edit_link_;

  DISALLOW_COPY_AND_ASSIGN(ProfileItemView);
};

ProfileItemView::ProfileItemView(const AvatarMenuModel::Item& item,
                                 AvatarMenuBubbleView* parent)
    : views::CustomButton(parent),
      item_(item),
      parent_(parent) {
  set_notify_enter_exit_on_child(true);

  image_view_ = new ProfileImageView();
  gfx::ImageSkia profile_icon = *item_.icon.ToImageSkia();
  if (item_.active)
    image_view_->SetImage(GetBadgedIcon(profile_icon));
  else
    image_view_->SetImage(profile_icon);
  AddChildView(image_view_);

  // Add a label to show the profile name.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  name_label_ = new views::Label(item_.name,
                                 rb.GetFont(item_.active ?
                                            ui::ResourceBundle::BoldFont :
                                            ui::ResourceBundle::BaseFont));
  name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(name_label_);

  // Add a label to show the sync state.
  sync_state_label_ = new views::Label(item_.sync_state);
  if (item_.signed_in)
    sync_state_label_->SetElideBehavior(views::Label::ELIDE_AS_EMAIL);
  sync_state_label_->SetFont(rb.GetFont(ui::ResourceBundle::SmallFont));
  sync_state_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  sync_state_label_->SetEnabled(false);
  AddChildView(sync_state_label_);

  // Add an edit profile link.
  edit_link_ = new EditProfileLink(
      l10n_util::GetStringUTF16(IDS_PROFILES_EDIT_PROFILE_LINK), this);
  edit_link_->set_listener(parent);
  edit_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
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
    const gfx::ImageSkia& icon = image_view_->GetImage();
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

void ProfileItemView::OnMouseEntered(const ui::MouseEvent& event) {
  views::CustomButton::OnMouseEntered(event);
  OnHighlightStateChanged();
}

void ProfileItemView::OnMouseExited(const ui::MouseEvent& event) {
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
  const SkColor color = IsHighlighted() ? kHighlightColor : parent_->color();
  set_background(views::Background::CreateSolidBackground(color));
  name_label_->SetBackgroundColor(color);
  sync_state_label_->SetBackgroundColor(color);
  edit_link_->SetBackgroundColor(color);

  bool show_edit = IsHighlighted() && item_.active;
  sync_state_label_->SetVisible(!show_edit);
  edit_link_->SetVisible(show_edit);
  SchedulePaint();
}

void ProfileItemView::OnFocusStateChanged(bool has_focus) {
  if (!has_focus && state() != views::CustomButton::STATE_DISABLED)
    SetState(views::CustomButton::STATE_NORMAL);
  OnHighlightStateChanged();
}

// static
gfx::ImageSkia ProfileItemView::GetBadgedIcon(const gfx::ImageSkia& icon) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* badge = rb.GetImageNamed(
      IDR_PROFILE_SELECTED).ToImageSkia();
  gfx::Size icon_size = GetCenteredAndScaledRect(icon.width(), icon.height(),
      0, 0, profiles::kAvatarIconWidth, kItemHeight).size();
  gfx::CanvasImageSource* source =
      new BadgeImageSource(icon, icon_size, *badge);
  // ImageSkia takes ownership of |source|.
  return gfx::ImageSkia(source, source->size());
}

bool ProfileItemView::IsHighlighted() {
  return state() == views::CustomButton::STATE_PRESSED ||
         state() == views::CustomButton::STATE_HOVERED ||
         edit_link_->state() == views::CustomButton::STATE_PRESSED ||
         edit_link_->state() == views::CustomButton::STATE_HOVERED ||
         HasFocus() ||
         edit_link_->HasFocus();
}

}  // namespace


// AvatarMenuBubbleView -------------------------------------------------------

// static
AvatarMenuBubbleView* AvatarMenuBubbleView::avatar_bubble_ = NULL;

// static
void AvatarMenuBubbleView::ShowBubble(
    views::View* anchor_view,
    views::BubbleBorder::ArrowLocation arrow_location,
    views::BubbleBorder::BubbleAlignment border_alignment,
    const gfx::Rect& anchor_rect,
    Browser* browser) {
  if (IsShowing())
    return;

  DCHECK(chrome::IsCommandEnabled(browser, IDC_SHOW_AVATAR_MENU));
  avatar_bubble_ = new AvatarMenuBubbleView(
      anchor_view, arrow_location, anchor_rect, browser);
  views::BubbleDelegateView::CreateBubble(avatar_bubble_);
  avatar_bubble_->SetAlignment(border_alignment);
  avatar_bubble_->Show();
}

// static
bool AvatarMenuBubbleView::IsShowing() {
  return avatar_bubble_ != NULL;
}

// static
void AvatarMenuBubbleView::Hide() {
  if (IsShowing())
    avatar_bubble_->GetWidget()->Close();
}

AvatarMenuBubbleView::AvatarMenuBubbleView(
    views::View* anchor_view,
    views::BubbleBorder::ArrowLocation arrow_location,
    const gfx::Rect& anchor_rect,
    Browser* browser)
    : BubbleDelegateView(anchor_view, arrow_location),
      anchor_rect_(anchor_rect),
      browser_(browser),
      separator_(NULL),
      add_profile_link_(NULL) {
  avatar_menu_model_.reset(new AvatarMenuModel(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      this, browser_));
}

AvatarMenuBubbleView::~AvatarMenuBubbleView() {
}

gfx::Size AvatarMenuBubbleView::GetPreferredSize() {
  int max_width = 0;
  int total_height = 0;
  for (size_t i = 0; i < item_views_.size(); ++i) {
    gfx::Size size = item_views_[i]->GetPreferredSize();
    max_width = std::max(max_width, size.width());
    total_height += size.height() + kItemMarginY;
  }

  if (add_profile_link_) {
    total_height += kSeparatorPaddingY * 2 +
                    separator_->GetPreferredSize().height();

    gfx::Size add_profile_size = add_profile_link_->GetPreferredSize();
    max_width = std::max(max_width,
        add_profile_size.width() + profiles::kAvatarIconWidth + kIconMarginX);
    total_height += add_profile_link_->GetPreferredSize().height();
  }

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

  if (add_profile_link_) {
    y += kSeparatorPaddingY;
    int separator_height = separator_->GetPreferredSize().height();
    separator_->SetBounds(0, y, width(), separator_height);
    y += kSeparatorPaddingY + separator_height;

    add_profile_link_->SetBounds(profiles::kAvatarIconWidth + kIconMarginX, y,
        width(), add_profile_link_->GetPreferredSize().height());
  }
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
                                         const ui::Event& event) {
  for (size_t i = 0; i < item_views_.size(); ++i) {
    ProfileItemView* item_view = static_cast<ProfileItemView*>(item_views_[i]);
    if (sender == item_view) {
      // Clicking on the active profile shouldn't do anything.
      if (!item_view->item().active) {
        avatar_menu_model_->SwitchToProfile(
            i, ui::DispositionFromEventFlags(event.flags()) == NEW_WINDOW);
      }
      break;
    }
  }
}

void AvatarMenuBubbleView::LinkClicked(views::Link* source, int event_flags) {
  if (source == add_profile_link_) {
    avatar_menu_model_->AddNewProfile(ProfileMetrics::ADD_NEW_USER_ICON);
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
  AddAccelerator(ui::Accelerator(ui::VKEY_DOWN, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_UP, ui::EF_NONE));
}

void AvatarMenuBubbleView::WindowClosing() {
  DCHECK_EQ(avatar_bubble_, this);
  avatar_bubble_ = NULL;
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
    ProfileItemView* item_view = new ProfileItemView(item, this);
    item_view->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_PROFILES_SWITCH_TO_PROFILE_ACCESSIBLE_NAME, item.name));
    item_view->set_focusable(true);
    AddChildView(item_view);
    item_views_.push_back(item_view);
  }

  if (avatar_menu_model_->ShouldShowAddNewProfileLink()) {
    separator_ = new views::Separator();
    AddChildView(separator_);

    add_profile_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_PROFILES_CREATE_NEW_PROFILE_LINK));
    add_profile_link_->set_listener(this);
    add_profile_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    add_profile_link_->SetBackgroundColor(color());
    AddChildView(add_profile_link_);
  }

  // If the bubble has already been shown then resize and reposition the bubble.
  Layout();
  if (GetBubbleFrameView())
    SizeToContents();
}
