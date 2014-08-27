// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_menu_bubble_view.h"

#include <algorithm>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
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
  explicit EditProfileLink(const base::string16& title,
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

EditProfileLink::EditProfileLink(const base::string16& title,
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


}  // namespace

// ProfileItemView ------------------------------------------------------------

// Control that shows information about a single profile.
class ProfileItemView : public views::CustomButton,
                        public HighlightDelegate {
 public:
  ProfileItemView(const AvatarMenu::Item& item,
                  AvatarMenuBubbleView* parent,
                  AvatarMenu* menu);

  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  virtual void OnHighlightStateChanged() OVERRIDE;
  virtual void OnFocusStateChanged(bool has_focus) OVERRIDE;

  const AvatarMenu::Item& item() const { return item_; }
  EditProfileLink* edit_link() { return edit_link_; }

 private:
  gfx::ImageSkia GetBadgedIcon(const gfx::ImageSkia& icon);

  bool IsHighlighted();

  AvatarMenu::Item item_;
  AvatarMenuBubbleView* parent_;
  AvatarMenu* menu_;
  views::ImageView* image_view_;
  views::Label* name_label_;
  views::Label* sync_state_label_;
  EditProfileLink* edit_link_;

  DISALLOW_COPY_AND_ASSIGN(ProfileItemView);
};

ProfileItemView::ProfileItemView(const AvatarMenu::Item& item,
                                 AvatarMenuBubbleView* parent,
                                 AvatarMenu* menu)
    : views::CustomButton(parent),
      item_(item),
      parent_(parent),
      menu_(menu) {
  set_notify_enter_exit_on_child(true);

  // Create an image-view for the profile. Make sure it ignores events so that
  // the parent can receive the events instead.
  image_view_ = new views::ImageView();
  image_view_->set_interactive(false);

  // GetSizedAvatarIcon will resize the icon in case it's too large.
  const gfx::ImageSkia profile_icon = *profiles::GetSizedAvatarIcon(item_.icon,
      false, profiles::kAvatarIconWidth, kItemHeight).ToImageSkia();
  if (item_.active || item_.signin_required)
    image_view_->SetImage(GetBadgedIcon(profile_icon));
  else
    image_view_->SetImage(profile_icon);
  AddChildView(image_view_);

  // Add a label to show the profile name.
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  name_label_ = new views::Label(item_.name,
                                 rb->GetFontList(item_.active ?
                                                 ui::ResourceBundle::BoldFont :
                                                 ui::ResourceBundle::BaseFont));
  name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(name_label_);

  // Add a label to show the sync state.
  sync_state_label_ = new views::Label(item_.sync_state);
  if (item_.signed_in)
    sync_state_label_->SetElideBehavior(gfx::ELIDE_EMAIL);
  sync_state_label_->SetFontList(
      rb->GetFontList(ui::ResourceBundle::SmallFont));
  sync_state_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  sync_state_label_->SetEnabled(false);
  AddChildView(sync_state_label_);

  // Add an edit profile link.
  edit_link_ = new EditProfileLink(
      l10n_util::GetStringUTF16(IDS_PROFILES_EDIT_PROFILE_LINK), this);
  edit_link_->set_listener(parent);
  edit_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(edit_link_);

  OnHighlightStateChanged();
}

gfx::Size ProfileItemView::GetPreferredSize() const {
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

  bool show_edit = IsHighlighted() && item_.active &&
      menu_->ShouldShowEditProfileLink();
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
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* badge = NULL;

  if (item_.active)
    badge = rb->GetImageSkiaNamed(IDR_PROFILE_SELECTED);
  else if (item_.signin_required)  // TODO(bcwhite): create new icon
    badge = rb->GetImageSkiaNamed(IDR_OMNIBOX_HTTPS_VALID);
  else
    NOTREACHED();  // function should only be called if one of above is true

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


// ActionButtonView -----------------------------------------------------------

// A custom view that manages the "action" buttons at the bottom of the list
// of profiles.
class ActionButtonView : public views::View {
 public:
  ActionButtonView(views::ButtonListener* listener, Profile* profile);

 private:
  views::LabelButton* manage_button_;
  views::LabelButton* signout_button_;

  DISALLOW_COPY_AND_ASSIGN(ActionButtonView);
};


ActionButtonView::ActionButtonView(views::ButtonListener* listener,
                                   Profile* profile)
  : manage_button_(NULL),
    signout_button_(NULL) {
  std::string username;
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(profile);
  if (signin != NULL)
    username = signin->GetAuthenticatedUsername();

  manage_button_ = new views::LabelButton(
      listener, l10n_util::GetStringUTF16(IDS_PROFILES_MANAGE_PROFILES_BUTTON));
  manage_button_->SetStyle(views::Button::STYLE_BUTTON);
  manage_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PROFILES_MANAGE_PROFILES_BUTTON_TIP));
  manage_button_->set_tag(IDS_PROFILES_MANAGE_PROFILES_BUTTON);

  signout_button_ = new views::LabelButton(
      listener, l10n_util::GetStringUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON));
  signout_button_->SetStyle(views::Button::STYLE_BUTTON);
  if (username.empty()) {
    signout_button_->SetTooltipText(
        l10n_util::GetStringUTF16(
            IDS_PROFILES_PROFILE_SIGNOUT_BUTTON_TIP_UNAVAILABLE));
    signout_button_->SetEnabled(false);
  } else {
    signout_button_->SetTooltipText(
        l10n_util::GetStringFUTF16(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON_TIP,
                                   base::UTF8ToUTF16(username)));
  }
  signout_button_->set_tag(IDS_PROFILES_PROFILE_SIGNOUT_BUTTON);

  views::GridLayout* layout = new views::GridLayout(this);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  layout->AddView(signout_button_);
  layout->AddView(manage_button_);
  SetLayoutManager(layout);
}


// AvatarMenuBubbleView -------------------------------------------------------

// static
AvatarMenuBubbleView* AvatarMenuBubbleView::avatar_bubble_ = NULL;
bool AvatarMenuBubbleView::close_on_deactivate_for_testing_ = true;

// static
void AvatarMenuBubbleView::ShowBubble(
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    views::BubbleBorder::ArrowPaintType arrow_paint_type,
    views::BubbleBorder::BubbleAlignment border_alignment,
    const gfx::Rect& anchor_rect,
    Browser* browser) {
  if (IsShowing())
    return;

  DCHECK(chrome::IsCommandEnabled(browser, IDC_SHOW_AVATAR_MENU));
  avatar_bubble_ = new AvatarMenuBubbleView(
      anchor_view, arrow, anchor_rect, browser);
  views::BubbleDelegateView::CreateBubble(avatar_bubble_);
  avatar_bubble_->set_close_on_deactivate(close_on_deactivate_for_testing_);
  avatar_bubble_->SetBackgroundColors();
  avatar_bubble_->SetAlignment(border_alignment);
  avatar_bubble_->SetArrowPaintType(arrow_paint_type);
  avatar_bubble_->GetWidget()->Show();
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
    views::BubbleBorder::Arrow arrow,
    const gfx::Rect& anchor_rect,
    Browser* browser)
    : BubbleDelegateView(anchor_view, arrow),
      anchor_rect_(anchor_rect),
      browser_(browser),
      separator_(NULL),
      buttons_view_(NULL),
      supervised_user_info_(NULL),
      separator_switch_users_(NULL),
      expanded_(false) {
  avatar_menu_.reset(new AvatarMenu(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      this,
      browser_));
  avatar_menu_->RebuildMenu();
}

AvatarMenuBubbleView::~AvatarMenuBubbleView() {
}

gfx::Size AvatarMenuBubbleView::GetPreferredSize() const {
  const int kBubbleViewMinWidth = 175;
  gfx::Size preferred_size(kBubbleViewMinWidth, 0);
  for (size_t i = 0; i < item_views_.size(); ++i) {
    gfx::Size size = item_views_[i]->GetPreferredSize();
    preferred_size.Enlarge(0, size.height() + kItemMarginY);
    preferred_size.SetToMax(size);
  }

  if (buttons_view_) {
    preferred_size.Enlarge(
        0, kSeparatorPaddingY * 2 + separator_->GetPreferredSize().height());

    gfx::Size buttons_size = buttons_view_->GetPreferredSize();
    preferred_size.Enlarge(0, buttons_size.height());
    preferred_size.SetToMax(buttons_size);
  }


  if (supervised_user_info_) {
    // First handle the switch profile link because it can still affect the
    // preferred width.
    gfx::Size size = switch_profile_link_->GetPreferredSize();
    preferred_size.Enlarge(0, size.height());
    preferred_size.SetToMax(size);

    // Add the height of the two separators.
    preferred_size.Enlarge(
        0,
        kSeparatorPaddingY * 4 + separator_->GetPreferredSize().height() * 2);
  }

  const int kBubbleViewMaxWidth = 800;
  preferred_size.SetToMin(
      gfx::Size(kBubbleViewMaxWidth, preferred_size.height()));

  // We have to do this after the final width is calculated, since the label
  // will wrap based on the width.
  if (supervised_user_info_) {
    int remaining_width =
        preferred_size.width() - icon_view_->GetPreferredSize().width() -
        views::kRelatedControlSmallHorizontalSpacing;
    preferred_size.Enlarge(
        0,
        supervised_user_info_->GetHeightForWidth(remaining_width) +
            kItemMarginY);
  }

  return preferred_size;
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

  int separator_height = 0;
  if (buttons_view_ || supervised_user_info_) {
    separator_height = separator_->GetPreferredSize().height();
    y += kSeparatorPaddingY;
    separator_->SetBounds(0, y, width(), separator_height);
    y += kSeparatorPaddingY + separator_height;
  }

  if (buttons_view_) {
    buttons_view_->SetBounds(0, y,
        width(), buttons_view_->GetPreferredSize().height());
  } else if (supervised_user_info_) {
    gfx::Size icon_size = icon_view_->GetPreferredSize();
    gfx::Rect icon_bounds(0, y, icon_size.width(), icon_size.height());
    icon_view_->SetBoundsRect(icon_bounds);
    int info_width = width() - icon_bounds.right() -
                     views::kRelatedControlSmallHorizontalSpacing;
    int height = supervised_user_info_->GetHeightForWidth(info_width);
    supervised_user_info_->SetBounds(
        icon_bounds.right() + views::kRelatedControlSmallHorizontalSpacing,
        y, info_width, height);
    y += height + kItemMarginY + kSeparatorPaddingY;
    separator_switch_users_->SetBounds(0, y, width(), separator_height);
    y += separator_height + kSeparatorPaddingY;
    int link_height = switch_profile_link_->GetPreferredSize().height();
    switch_profile_link_->SetBounds(0, y, width(), link_height);
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
  if (sender->tag() == IDS_PROFILES_MANAGE_PROFILES_BUTTON) {
    std::string subpage = chrome::kSearchUsersSubPage;
    chrome::ShowSettingsSubPage(browser_, subpage);
    return;
  } else if (sender->tag() == IDS_PROFILES_PROFILE_SIGNOUT_BUTTON) {
    profiles::LockProfile(browser_->profile());
    return;
  }

  for (size_t i = 0; i < item_views_.size(); ++i) {
    ProfileItemView* item_view = item_views_[i];
    if (sender == item_view) {
      // Clicking on the active profile shouldn't do anything.
      if (!item_view->item().active) {
        avatar_menu_->SwitchToProfile(
            i, ui::DispositionFromEventFlags(event.flags()) == NEW_WINDOW,
            ProfileMetrics::SWITCH_PROFILE_ICON);
      }
      break;
    }
  }
}

void AvatarMenuBubbleView::LinkClicked(views::Link* source, int event_flags) {
  if (source == buttons_view_) {
    avatar_menu_->AddNewProfile(ProfileMetrics::ADD_NEW_USER_ICON);
    return;
  }
  if (source == switch_profile_link_) {
    expanded_ = true;
    OnAvatarMenuChanged(avatar_menu_.get());
    return;
  }

  for (size_t i = 0; i < item_views_.size(); ++i) {
    ProfileItemView* item_view = item_views_[i];
    if (source == item_view->edit_link()) {
      avatar_menu_->EditProfile(i);
      return;
    }
  }
}

gfx::Rect AvatarMenuBubbleView::GetAnchorRect() const {
  return anchor_rect_;
}

void AvatarMenuBubbleView::Init() {
  // Build the menu for the first time.
  OnAvatarMenuChanged(avatar_menu_.get());
  AddAccelerator(ui::Accelerator(ui::VKEY_DOWN, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_UP, ui::EF_NONE));
}

void AvatarMenuBubbleView::WindowClosing() {
  DCHECK_EQ(avatar_bubble_, this);
  avatar_bubble_ = NULL;
}

void AvatarMenuBubbleView::InitMenuContents(
    AvatarMenu* avatar_menu) {
  for (size_t i = 0; i < avatar_menu->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu->GetItemAt(i);
    ProfileItemView* item_view = new ProfileItemView(item,
                                                     this,
                                                     avatar_menu_.get());
    item_view->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_PROFILES_SWITCH_TO_PROFILE_ACCESSIBLE_NAME, item.name));
    item_view->SetFocusable(true);
    AddChildView(item_view);
    item_views_.push_back(item_view);
  }

  if (avatar_menu_->ShouldShowAddNewProfileLink()) {
    views::Link* add_profile_link = new views::Link(
        l10n_util::GetStringUTF16(IDS_PROFILES_CREATE_NEW_PROFILE_LINK));
    add_profile_link->set_listener(this);
    add_profile_link->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    add_profile_link->SetBackgroundColor(color());
    separator_ = new views::Separator(views::Separator::HORIZONTAL);
    AddChildView(separator_);
    buttons_view_ = add_profile_link;
    AddChildView(buttons_view_);
  }
}

void AvatarMenuBubbleView::InitSupervisedUserContents(
    AvatarMenu* avatar_menu) {
  // Show the profile of the supervised user.
  size_t active_index = avatar_menu->GetActiveProfileIndex();
  const AvatarMenu::Item& item =
      avatar_menu->GetItemAt(active_index);
  ProfileItemView* item_view = new ProfileItemView(item,
                                                   this,
                                                   avatar_menu_.get());
  item_view->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_PROFILES_SWITCH_TO_PROFILE_ACCESSIBLE_NAME, item.name));
  item_views_.push_back(item_view);
  AddChildView(item_view);
  separator_ = new views::Separator(views::Separator::HORIZONTAL);
  AddChildView(separator_);

  // Add information about supervised users.
  supervised_user_info_ =
      new views::Label(avatar_menu_->GetSupervisedUserInformation(),
                       ui::ResourceBundle::GetSharedInstance().GetFontList(
                           ui::ResourceBundle::SmallFont));
  supervised_user_info_->SetMultiLine(true);
  supervised_user_info_->SetAllowCharacterBreak(true);
  supervised_user_info_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  supervised_user_info_->SetBackgroundColor(color());
  AddChildView(supervised_user_info_);

  // Add the supervised user icon.
  icon_view_ = new views::ImageView();
  icon_view_->SetImage(avatar_menu_->GetSupervisedUserIcon().ToImageSkia());
  AddChildView(icon_view_);

  // Add a link for switching profiles.
  separator_switch_users_ = new views::Separator(views::Separator::HORIZONTAL);
  AddChildView(separator_switch_users_);
  switch_profile_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_PROFILES_SWITCH_PROFILE_LINK));
  switch_profile_link_->set_listener(this);
  switch_profile_link_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  switch_profile_link_->SetBackgroundColor(color());
  AddChildView(switch_profile_link_);
}

void AvatarMenuBubbleView::OnAvatarMenuChanged(
    AvatarMenu* avatar_menu) {
  // Unset all our child view references and call RemoveAllChildViews() which
  // will actually delete them.
  buttons_view_ = NULL;
  supervised_user_info_ = NULL;
  item_views_.clear();
  RemoveAllChildViews(true);

  if (avatar_menu_->GetSupervisedUserInformation().empty() || expanded_)
    InitMenuContents(avatar_menu);
  else
    InitSupervisedUserContents(avatar_menu);

  // If the bubble has already been shown then resize and reposition the bubble.
  Layout();
  if (GetBubbleFrameView())
    SizeToContents();
}

void AvatarMenuBubbleView::SetBackgroundColors() {
  for (size_t i = 0; i < item_views_.size(); ++i) {
    item_views_[i]->OnHighlightStateChanged();
  }
}
