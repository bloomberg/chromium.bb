// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/tray_user.h"

#include <algorithm>
#include <climits>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/multi_profile_uma.h"
#include "ash/popup_message.h"
#include "ash/root_window_controller.h"
#include "ash/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_popup_label_button.h"
#include "ash/system/tray/tray_popup_label_button_border.h"
#include "ash/system/tray/tray_utils.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/border.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

namespace {

const int kUserDetailsVerticalPadding = 5;
const int kUserCardVerticalPadding = 10;
const int kProfileRoundedCornerRadius = 2;
const int kUserIconSize = 27;
const int kUserIconLargeSize = 32;
const int kUserIconLargeCornerRadius = 2;
const int kUserLabelToIconPadding = 5;

// When a hover border is used, it is starting this many pixels before the icon
// position.
const int kTrayUserTileHoverBorderInset = 10;

// The border color of the user button.
const SkColor kBorderColor = 0xffdcdcdc;

// The invisible word joiner character, used as a marker to indicate the start
// and end of the user's display name in the public account user card's text.
const base::char16 kDisplayNameMark[] = { 0x2060, 0 };

const int kPublicAccountLogoutButtonBorderImagesNormal[] = {
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
};

const int kPublicAccountLogoutButtonBorderImagesHovered[] = {
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_HOVER_BACKGROUND,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_PUBLIC_ACCOUNT_LOGOUT_BUTTON_BORDER,
};

// Offsetting the popup message relative to the tray menu.
const int kPopupMessageOffset = 25;

// Switch to a user with the given |user_index|.
void SwitchUser(ash::MultiProfileIndex user_index) {
  // Do not switch users when the log screen is presented.
  if (ash::Shell::GetInstance()->session_state_delegate()->
          IsUserSessionBlocked())
    return;

  DCHECK(user_index > 0);
  ash::SessionStateDelegate* delegate =
      ash::Shell::GetInstance()->session_state_delegate();
  ash::MultiProfileUMA::RecordSwitchActiveUser(
      ash::MultiProfileUMA::SWITCH_ACTIVE_USER_BY_TRAY);
  delegate->SwitchActiveUser(delegate->GetUserID(user_index));
}

}  // namespace

namespace ash {
namespace tray {

// A custom image view with rounded edges.
class RoundedImageView : public views::View {
 public:
  // Constructs a new rounded image view with rounded corners of radius
  // |corner_radius|. If |active_user| is set, the icon will be drawn in
  // full colors - otherwise it will fade into the background.
  RoundedImageView(int corner_radius, bool active_user);
  virtual ~RoundedImageView();

  // Set the image that should be displayed. The image contents is copied to the
  // receiver's image.
  void SetImage(const gfx::ImageSkia& img, const gfx::Size& size);

  // Set the radii of the corners independently.
  void SetCornerRadii(int top_left,
                      int top_right,
                      int bottom_right,
                      int bottom_left);

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  gfx::ImageSkia image_;
  gfx::ImageSkia resized_;
  gfx::Size image_size_;
  int corner_radius_[4];

  // True if the given user is the active user and the icon should get
  // painted as active.
  bool active_user_;

  DISALLOW_COPY_AND_ASSIGN(RoundedImageView);
};

// The user details shown in public account mode. This is essentially a label
// but with custom painting code as the text is styled with multiple colors and
// contains a link.
class PublicAccountUserDetails : public views::View,
                                 public views::LinkListener {
 public:
  PublicAccountUserDetails(SystemTrayItem* owner, int used_width);
  virtual ~PublicAccountUserDetails();

 private:
  // Overridden from views::View.
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::LinkListener.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Calculate a preferred size that ensures the label text and the following
  // link do not wrap over more than three lines in total for aesthetic reasons
  // if possible.
  void CalculatePreferredSize(SystemTrayItem* owner, int used_width);

  base::string16 text_;
  views::Link* learn_more_;
  gfx::Size preferred_size_;
  ScopedVector<gfx::RenderText> lines_;

  DISALLOW_COPY_AND_ASSIGN(PublicAccountUserDetails);
};

// The button which holds the user card in case of multi profile.
class UserCard : public views::CustomButton {
 public:
  UserCard(views::ButtonListener* listener, bool active_user);
  virtual ~UserCard();

  // Called when the border should remain even in the non highlighted state.
  void ForceBorderVisible(bool show);

  // Overridden from views::View
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;

  // Check if the item is hovered.
  bool is_hovered_for_test() {return button_hovered_; }

 private:
  // Change the hover/active state of the "button" when the status changes.
  void ShowActive();

  // True if this is the active user.
  bool is_active_user_;

  // True if button is hovered.
  bool button_hovered_;

  // True if the border should be visible.
  bool show_border_;

  DISALLOW_COPY_AND_ASSIGN(UserCard);
};

class UserViewMouseWatcherHost : public views::MouseWatcherHost {
public:
 explicit UserViewMouseWatcherHost(const gfx::Rect& screen_area)
     : screen_area_(screen_area) {}
 virtual ~UserViewMouseWatcherHost() {}

 // Implementation of MouseWatcherHost.
 virtual bool Contains(const gfx::Point& screen_point,
                       views::MouseWatcherHost::MouseEventType type) OVERRIDE {
   return screen_area_.Contains(screen_point);
 }

private:
 gfx::Rect screen_area_;

 DISALLOW_COPY_AND_ASSIGN(UserViewMouseWatcherHost);
};

// The view of a user item.
class UserView : public views::View,
                 public views::ButtonListener,
                 public views::MouseWatcherListener {
 public:
  UserView(SystemTrayItem* owner,
           ash::user::LoginStatus login,
           MultiProfileIndex index);
  virtual ~UserView();

  // Overridden from MouseWatcherListener:
  virtual void MouseMovedOutOfHost() OVERRIDE;

  TrayUser::TestState GetStateForTest() const;
  gfx::Rect GetBoundsInScreenOfUserButtonForTest();

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void Layout() OVERRIDE;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  void AddLogoutButton(user::LoginStatus login);
  void AddUserCard(SystemTrayItem* owner, user::LoginStatus login);

  // Create a user icon representation for the user card.
  views::View* CreateIconForUserCard(user::LoginStatus login);

  // Create the additional user card content for the retail logged in mode.
  void AddLoggedInRetailModeUserCardContent();

  // Create the additional user card content for the public mode.
  void AddLoggedInPublicModeUserCardContent(SystemTrayItem* owner);

  // Create the menu option to add another user. If |disabled| is set the user
  // cannot actively click on the item.
  void ToggleAddUserMenuOption();

  // Returns true when multi profile is supported.
  bool SupportsMultiProfile();

  MultiProfileIndex multiprofile_index_;
  // The view of the user card.
  views::View* user_card_view_;

  // This is the owner system tray item of this view.
  SystemTrayItem* owner_;

  // True if |user_card_view_| is a |UserView| - otherwise it is only a
  // |views::View|.
  bool is_user_card_;
  views::View* logout_button_;
  scoped_ptr<PopupMessage> popup_message_;
  scoped_ptr<views::Widget> add_menu_option_;

  // True when the add user panel is visible but not activatable.
  bool add_user_visible_but_disabled_;

  // The mouse watcher which takes care of out of window hover events.
  scoped_ptr<views::MouseWatcher> mouse_watcher_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

// The menu item view which gets shown when the user clicks in multi profile
// mode onto the user item.
class AddUserView : public views::CustomButton,
                    public views::ButtonListener {
 public:
  // The |owner| is the view for which this view gets created. The |listener|
  // will get notified when this item gets clicked.
  AddUserView(UserCard* owner, views::ButtonListener* listener);
  virtual ~AddUserView();

  // Get the anchor view for a message.
  views::View* anchor() { return anchor_; }

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void Layout() OVERRIDE;

  // Create the additional client content for this item.
  void AddContent();

  // This is the content we create and show.
  views::View* add_user_;

  // This listener will get informed when someone clicks on this button.
  views::ButtonListener* listener_;

  // This is the owner view of this item.
  UserCard* owner_;

  // The anchor view for targetted bubble messages.
  views::View* anchor_;

  DISALLOW_COPY_AND_ASSIGN(AddUserView);
};

RoundedImageView::RoundedImageView(int corner_radius, bool active_user)
    : active_user_(active_user) {
  for (int i = 0; i < 4; ++i)
    corner_radius_[i] = corner_radius;
}

RoundedImageView::~RoundedImageView() {}

void RoundedImageView::SetImage(const gfx::ImageSkia& img,
                                const gfx::Size& size) {
  image_ = img;
  image_size_ = size;

  // Try to get the best image quality for the avatar.
  resized_ = gfx::ImageSkiaOperations::CreateResizedImage(image_,
      skia::ImageOperations::RESIZE_BEST, size);
  if (GetWidget() && visible()) {
    PreferredSizeChanged();
    SchedulePaint();
  }
}

void RoundedImageView::SetCornerRadii(int top_left,
                                      int top_right,
                                      int bottom_right,
                                      int bottom_left) {
  corner_radius_[0] = top_left;
  corner_radius_[1] = top_right;
  corner_radius_[2] = bottom_right;
  corner_radius_[3] = bottom_left;
}

gfx::Size RoundedImageView::GetPreferredSize() {
  return gfx::Size(image_size_.width() + GetInsets().width(),
                   image_size_.height() + GetInsets().height());
}

void RoundedImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  gfx::Rect image_bounds(size());
  image_bounds.ClampToCenteredSize(GetPreferredSize());
  image_bounds.Inset(GetInsets());
  const SkScalar kRadius[8] = {
    SkIntToScalar(corner_radius_[0]),
    SkIntToScalar(corner_radius_[0]),
    SkIntToScalar(corner_radius_[1]),
    SkIntToScalar(corner_radius_[1]),
    SkIntToScalar(corner_radius_[2]),
    SkIntToScalar(corner_radius_[2]),
    SkIntToScalar(corner_radius_[3]),
    SkIntToScalar(corner_radius_[3])
  };
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(image_bounds), kRadius);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setXfermodeMode(active_user_ ? SkXfermode::kSrcOver_Mode :
                                       SkXfermode::kLuminosity_Mode);
  canvas->DrawImageInPath(resized_, image_bounds.x(), image_bounds.y(),
                          path, paint);
}

PublicAccountUserDetails::PublicAccountUserDetails(SystemTrayItem* owner,
                                                   int used_width)
    : learn_more_(NULL) {
  const int inner_padding =
      kTrayPopupPaddingHorizontal - kTrayPopupPaddingBetweenItems;
  const bool rtl = base::i18n::IsRTL();
  SetBorder(views::Border::CreateEmptyBorder(kUserDetailsVerticalPadding,
                                             rtl ? 0 : inner_padding,
                                             kUserDetailsVerticalPadding,
                                             rtl ? inner_padding : 0));

  // Retrieve the user's display name and wrap it with markers.
  // Note that since this is a public account it always has to be the primary
  // user.
  base::string16 display_name =
      Shell::GetInstance()->session_state_delegate()->GetUserDisplayName(0);
  base::RemoveChars(display_name, kDisplayNameMark, &display_name);
  display_name = kDisplayNameMark[0] + display_name + kDisplayNameMark[0];
  // Retrieve the domain managing the device and wrap it with markers.
  base::string16 domain = base::UTF8ToUTF16(
      Shell::GetInstance()->system_tray_delegate()->GetEnterpriseDomain());
  base::RemoveChars(domain, kDisplayNameMark, &domain);
  base::i18n::WrapStringWithLTRFormatting(&domain);
  // Retrieve the label text, inserting the display name and domain.
  text_ = l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_PUBLIC_LABEL,
                                     display_name, domain);

  learn_more_ = new views::Link(l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE));
  learn_more_->SetUnderline(false);
  learn_more_->set_listener(this);
  AddChildView(learn_more_);

  CalculatePreferredSize(owner, used_width);
}

PublicAccountUserDetails::~PublicAccountUserDetails() {}

void PublicAccountUserDetails::Layout() {
  lines_.clear();
  const gfx::Rect contents_area = GetContentsBounds();
  if (contents_area.IsEmpty())
    return;

  // Word-wrap the label text.
  const gfx::FontList font_list;
  std::vector<base::string16> lines;
  gfx::ElideRectangleText(text_, font_list, contents_area.width(),
                          contents_area.height(), gfx::ELIDE_LONG_WORDS,
                          &lines);
  // Loop through the lines, creating a renderer for each.
  gfx::Point position = contents_area.origin();
  gfx::Range display_name(gfx::Range::InvalidRange());
  for (std::vector<base::string16>::const_iterator it = lines.begin();
       it != lines.end(); ++it) {
    gfx::RenderText* line = gfx::RenderText::CreateInstance();
    line->SetDirectionalityMode(gfx::DIRECTIONALITY_FROM_UI);
    line->SetText(*it);
    const gfx::Size size(contents_area.width(), line->GetStringSize().height());
    line->SetDisplayRect(gfx::Rect(position, size));
    position.set_y(position.y() + size.height());

    // Set the default text color for the line.
    line->SetColor(kPublicAccountUserCardTextColor);

    // If a range of the line contains the user's display name, apply a custom
    // text color to it.
    if (display_name.is_empty())
      display_name.set_start(it->find(kDisplayNameMark));
    if (!display_name.is_empty()) {
      display_name.set_end(
          it->find(kDisplayNameMark, display_name.start() + 1));
      gfx::Range line_range(0, it->size());
      line->ApplyColor(kPublicAccountUserCardNameColor,
                       display_name.Intersect(line_range));
      // Update the range for the next line.
      if (display_name.end() >= line_range.end())
        display_name.set_start(0);
      else
        display_name = gfx::Range::InvalidRange();
    }

    lines_.push_back(line);
  }

  // Position the link after the label text, separated by a space. If it does
  // not fit onto the last line of the text, wrap the link onto its own line.
  const gfx::Size last_line_size = lines_.back()->GetStringSize();
  const int space_width =
      gfx::GetStringWidth(base::ASCIIToUTF16(" "), font_list);
  const gfx::Size link_size = learn_more_->GetPreferredSize();
  if (contents_area.width() - last_line_size.width() >=
      space_width + link_size.width()) {
    position.set_x(position.x() + last_line_size.width() + space_width);
    position.set_y(position.y() - last_line_size.height());
  }
  position.set_y(position.y() - learn_more_->GetInsets().top());
  gfx::Rect learn_more_bounds(position, link_size);
  learn_more_bounds.Intersect(contents_area);
  if (base::i18n::IsRTL()) {
    const gfx::Insets insets = GetInsets();
    learn_more_bounds.Offset(insets.right() - insets.left(), 0);
  }
  learn_more_->SetBoundsRect(learn_more_bounds);
}

gfx::Size PublicAccountUserDetails::GetPreferredSize() {
  return preferred_size_;
}

void PublicAccountUserDetails::OnPaint(gfx::Canvas* canvas) {
  for (ScopedVector<gfx::RenderText>::const_iterator it = lines_.begin();
       it != lines_.end(); ++it) {
    (*it)->Draw(canvas);
  }
  views::View::OnPaint(canvas);
}

void PublicAccountUserDetails::LinkClicked(views::Link* source,
                                           int event_flags) {
  DCHECK_EQ(source, learn_more_);
  Shell::GetInstance()->system_tray_delegate()->ShowPublicAccountInfo();
}

void PublicAccountUserDetails::CalculatePreferredSize(SystemTrayItem* owner,
                                                      int used_width) {
  const gfx::FontList font_list;
  const gfx::Size link_size = learn_more_->GetPreferredSize();
  const int space_width =
      gfx::GetStringWidth(base::ASCIIToUTF16(" "), font_list);
  const gfx::Insets insets = GetInsets();
  views::TrayBubbleView* bubble_view =
      owner->system_tray()->GetSystemBubble()->bubble_view();
  int min_width = std::max(
      link_size.width(),
      bubble_view->GetPreferredSize().width() - (used_width + insets.width()));
  int max_width = std::min(
      gfx::GetStringWidth(text_, font_list) + space_width + link_size.width(),
      bubble_view->GetMaximumSize().width() - (used_width + insets.width()));
  // Do a binary search for the minimum width that ensures no more than three
  // lines are needed. The lower bound is the minimum of the current bubble
  // width and the width of the link (as no wrapping is permitted inside the
  // link). The upper bound is the maximum of the largest allowed bubble width
  // and the sum of the label text and link widths when put on a single line.
  std::vector<base::string16> lines;
  while (min_width < max_width) {
    lines.clear();
    const int width = (min_width + max_width) / 2;
    const bool too_narrow =
        gfx::ElideRectangleText(text_, font_list, width, INT_MAX,
                                gfx::TRUNCATE_LONG_WORDS, &lines) != 0;
    int line_count = lines.size();
    if (!too_narrow && line_count == 3 &&
        width - gfx::GetStringWidth(lines.back(), font_list) <=
            space_width + link_size.width())
      ++line_count;
    if (too_narrow || line_count > 3)
      min_width = width + 1;
    else
      max_width = width;
  }

  // Calculate the corresponding height and set the preferred size.
  lines.clear();
  gfx::ElideRectangleText(
      text_, font_list, min_width, INT_MAX, gfx::TRUNCATE_LONG_WORDS, &lines);
  int line_count = lines.size();
  if (min_width - gfx::GetStringWidth(lines.back(), font_list) <=
          space_width + link_size.width()) {
    ++line_count;
  }
  const int line_height = font_list.GetHeight();
  const int link_extra_height = std::max(
      link_size.height() - learn_more_->GetInsets().top() - line_height, 0);
  preferred_size_ = gfx::Size(
      min_width + insets.width(),
      line_count * line_height + link_extra_height + insets.height());

  bubble_view->SetWidth(preferred_size_.width() + used_width);
}

UserCard::UserCard(views::ButtonListener* listener, bool active_user)
    : CustomButton(listener),
      is_active_user_(active_user),
      button_hovered_(false),
      show_border_(false) {
  if (is_active_user_) {
    set_background(
        views::Background::CreateSolidBackground(kBackgroundColor));
    ShowActive();
  }
}

UserCard::~UserCard() {}

void UserCard::ForceBorderVisible(bool show) {
  show_border_ = show;
  ShowActive();
}

void UserCard::OnMouseEntered(const ui::MouseEvent& event) {
  if (is_active_user_) {
    button_hovered_ = true;
    background()->SetNativeControlColor(kHoverBackgroundColor);
    ShowActive();
  }
}

void UserCard::OnMouseExited(const ui::MouseEvent& event) {
  if (is_active_user_) {
    button_hovered_ = false;
    background()->SetNativeControlColor(kBackgroundColor);
    ShowActive();
  }
}

void UserCard::ShowActive() {
  int width = button_hovered_ || show_border_ ? 1 : 0;
  SetBorder(views::Border::CreateSolidSidedBorder(
      width, width, width, 1, kBorderColor));
  SchedulePaint();
}

UserView::UserView(SystemTrayItem* owner,
                   user::LoginStatus login,
                   MultiProfileIndex index)
    : multiprofile_index_(index),
      user_card_view_(NULL),
      owner_(owner),
      is_user_card_(false),
      logout_button_(NULL),
      add_user_visible_but_disabled_(false) {
  CHECK_NE(user::LOGGED_IN_NONE, login);
  if (!index) {
    // Only the logged in user will have a background. All other users will have
    // to allow the TrayPopupContainer highlighting the menu line.
    set_background(views::Background::CreateSolidBackground(
        login == user::LOGGED_IN_PUBLIC ? kPublicAccountBackgroundColor :
                                          kBackgroundColor));
  }
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                                        kTrayPopupPaddingBetweenItems));
  // The logout button must be added before the user card so that the user card
  // can correctly calculate the remaining available width.
  // Note that only the current multiprofile user gets a button.
  if (!multiprofile_index_)
    AddLogoutButton(login);
  AddUserCard(owner, login);
}

UserView::~UserView() {}

void UserView::MouseMovedOutOfHost() {
  popup_message_.reset();
  mouse_watcher_.reset();
  add_menu_option_.reset();
}

TrayUser::TestState UserView::GetStateForTest() const {
  if (add_menu_option_.get()) {
    return add_user_visible_but_disabled_ ? TrayUser::ACTIVE_BUT_DISABLED :
                                            TrayUser::ACTIVE;
  }

  if (!is_user_card_)
    return TrayUser::SHOWN;

  return static_cast<UserCard*>(user_card_view_)->is_hovered_for_test() ?
      TrayUser::HOVERED : TrayUser::SHOWN;
}

gfx::Rect UserView::GetBoundsInScreenOfUserButtonForTest() {
  DCHECK(user_card_view_);
  return user_card_view_->GetBoundsInScreen();
}

gfx::Size UserView::GetPreferredSize() {
  gfx::Size size = views::View::GetPreferredSize();
  // Only the active user panel will be forced to a certain height.
  if (!multiprofile_index_) {
    size.set_height(std::max(size.height(),
                             kTrayPopupItemHeight + GetInsets().height()));
  }
  return size;
}

int UserView::GetHeightForWidth(int width) {
  return GetPreferredSize().height();
}

void UserView::Layout() {
  gfx::Rect contents_area(GetContentsBounds());
  if (user_card_view_ && logout_button_) {
    // Give the logout button the space it requests.
    gfx::Rect logout_area = contents_area;
    logout_area.ClampToCenteredSize(logout_button_->GetPreferredSize());
    logout_area.set_x(contents_area.right() - logout_area.width());

    // Give the remaining space to the user card.
    gfx::Rect user_card_area = contents_area;
    int remaining_width = contents_area.width() - logout_area.width();
    if (SupportsMultiProfile()) {
      // In multiprofile case |user_card_view_| and |logout_button_| have to
      // have the same height.
      int y = std::min(user_card_area.y(), logout_area.y());
      int height = std::max(user_card_area.height(), logout_area.height());
      logout_area.set_y(y);
      logout_area.set_height(height);
      user_card_area.set_y(y);
      user_card_area.set_height(height);

      // In multiprofile mode we have also to increase the size of the card by
      // the size of the border to make it overlap with the logout button.
      user_card_area.set_width(std::max(0, remaining_width + 1));

      // To make the logout button symmetrical with the user card we also make
      // the button longer by the same size the hover area in front of the icon
      // got inset.
      logout_area.set_width(logout_area.width() +
                            kTrayUserTileHoverBorderInset);
    } else {
      // In all other modes we have to make sure that there is enough spacing
      // between the two.
      remaining_width -= kTrayPopupPaddingBetweenItems;
    }
    user_card_area.set_width(remaining_width);
    user_card_view_->SetBoundsRect(user_card_area);
    logout_button_->SetBoundsRect(logout_area);
  } else if (user_card_view_) {
    user_card_view_->SetBoundsRect(contents_area);
  } else if (logout_button_) {
    logout_button_->SetBoundsRect(contents_area);
  }
}

void UserView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == logout_button_) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        ash::UMA_STATUS_AREA_SIGN_OUT);
    Shell::GetInstance()->system_tray_delegate()->SignOut();
  } else if (sender == user_card_view_ && SupportsMultiProfile()) {
    if (!multiprofile_index_) {
      ToggleAddUserMenuOption();
    } else {
      SwitchUser(multiprofile_index_);
      // Since the user list is about to change the system menu should get
      // closed.
      owner_->system_tray()->CloseSystemBubble();
    }
  } else if (add_menu_option_.get() &&
             sender == add_menu_option_->GetContentsView()) {
    // Let the user add another account to the session.
    MultiProfileUMA::RecordSigninUser(MultiProfileUMA::SIGNIN_USER_BY_TRAY);
    Shell::GetInstance()->system_tray_delegate()->ShowUserLogin();
    owner_->system_tray()->CloseSystemBubble();
  } else {
    NOTREACHED();
  }
}

void UserView::AddLogoutButton(user::LoginStatus login) {
  const base::string16 title = user::GetLocalizedSignOutStringForStatus(login,
                                                                        true);
  TrayPopupLabelButton* logout_button = new TrayPopupLabelButton(this, title);
  logout_button->SetAccessibleName(title);
  logout_button_ = logout_button;
  // In public account mode, the logout button border has a custom color.
  if (login == user::LOGGED_IN_PUBLIC) {
    scoped_ptr<TrayPopupLabelButtonBorder> border(
        new TrayPopupLabelButtonBorder());
    border->SetPainter(false, views::Button::STATE_NORMAL,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesNormal));
    border->SetPainter(false, views::Button::STATE_HOVERED,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesHovered));
    border->SetPainter(false, views::Button::STATE_PRESSED,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesHovered));
    logout_button_->SetBorder(border.PassAs<views::Border>());
  }
  AddChildView(logout_button_);
}

void UserView::AddUserCard(SystemTrayItem* owner, user::LoginStatus login) {
  // Add padding around the panel.
  SetBorder(views::Border::CreateEmptyBorder(kUserCardVerticalPadding,
                                             kTrayPopupPaddingHorizontal,
                                             kUserCardVerticalPadding,
                                             kTrayPopupPaddingHorizontal));

  if (SupportsMultiProfile() && login != user::LOGGED_IN_RETAIL_MODE) {
    user_card_view_ = new UserCard(this, multiprofile_index_ == 0);
    is_user_card_ = true;
  } else {
    user_card_view_ = new views::View();
    is_user_card_ = false;
  }

  user_card_view_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0 , kTrayPopupPaddingBetweenItems));
  AddChildViewAt(user_card_view_, 0);

  if (login == user::LOGGED_IN_RETAIL_MODE) {
    AddLoggedInRetailModeUserCardContent();
    return;
  }

  // The entire user card should trigger hover (the inner items get disabled).
  user_card_view_->SetEnabled(true);
  user_card_view_->set_notify_enter_exit_on_child(true);

  if (login == user::LOGGED_IN_PUBLIC) {
    AddLoggedInPublicModeUserCardContent(owner);
    return;
  }

  views::View* icon = CreateIconForUserCard(login);
  user_card_view_->AddChildView(icon);

  // To allow the border to start before the icon, reduce the size before and
  // add an inset to the icon to get the spacing.
  if (multiprofile_index_ == 0 && SupportsMultiProfile()) {
    icon->SetBorder(views::Border::CreateEmptyBorder(
        0, kTrayUserTileHoverBorderInset, 0, 0));
    SetBorder(views::Border::CreateEmptyBorder(
        kUserCardVerticalPadding,
        kTrayPopupPaddingHorizontal - kTrayUserTileHoverBorderInset,
        kUserCardVerticalPadding,
        kTrayPopupPaddingHorizontal));
  }
  SessionStateDelegate* delegate =
      Shell::GetInstance()->session_state_delegate();
  views::Label* username = NULL;
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  if (!multiprofile_index_) {
    base::string16 user_name_string =
        login == user::LOGGED_IN_GUEST ?
            bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_GUEST_LABEL) :
            delegate->GetUserDisplayName(multiprofile_index_);
    if (!user_name_string.empty()) {
      username = new views::Label(user_name_string);
      username->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
  }

  views::Label* additional = NULL;
  if (login != user::LOGGED_IN_GUEST) {
    base::string16 user_email_string =
        login == user::LOGGED_IN_LOCALLY_MANAGED ?
            bundle.GetLocalizedString(
                IDS_ASH_STATUS_TRAY_LOCALLY_MANAGED_LABEL) :
            base::UTF8ToUTF16(delegate->GetUserEmail(multiprofile_index_));
    if (!user_email_string.empty()) {
      additional = new views::Label(user_email_string);
      additional->SetFontList(
          bundle.GetFontList(ui::ResourceBundle::SmallFont));
      additional->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
  }

  // Adjust text properties dependent on if it is an active or inactive user.
  if (multiprofile_index_) {
    // Fade the text of non active users to 50%.
    SkColor text_color = additional->enabled_color();
    text_color = SkColorSetA(text_color, SkColorGetA(text_color) / 2);
    if (additional)
      additional->SetDisabledColor(text_color);
    if (username)
      username->SetDisabledColor(text_color);
  }

  if (additional && username) {
    views::View* details = new views::View;
    details->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, kUserDetailsVerticalPadding, 0));
    details->AddChildView(username);
    details->AddChildView(additional);
    user_card_view_->AddChildView(details);
  } else {
    if (username)
      user_card_view_->AddChildView(username);
    if (additional)
      user_card_view_->AddChildView(additional);
  }
}

views::View* UserView::CreateIconForUserCard(user::LoginStatus login) {
  RoundedImageView* icon = new RoundedImageView(kProfileRoundedCornerRadius,
                                                multiprofile_index_ == 0);
  icon->SetEnabled(false);
  if (login == user::LOGGED_IN_GUEST) {
    icon->SetImage(*ui::ResourceBundle::GetSharedInstance().
        GetImageNamed(IDR_AURA_UBER_TRAY_GUEST_ICON).ToImageSkia(),
        gfx::Size(kUserIconSize, kUserIconSize));
  } else {
    SessionStateDelegate* delegate =
        Shell::GetInstance()->session_state_delegate();
    content::BrowserContext* context = delegate->GetBrowserContextByIndex(
        multiprofile_index_);
    icon->SetImage(delegate->GetUserImage(context),
                   gfx::Size(kUserIconSize, kUserIconSize));
  }
  return icon;
}

void UserView::AddLoggedInRetailModeUserCardContent() {
  views::Label* details = new views::Label;
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  details->SetText(
      bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_KIOSK_LABEL));
  details->SetBorder(views::Border::CreateEmptyBorder(0, 4, 0, 1));
  details->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  user_card_view_->AddChildView(details);
}

void UserView::AddLoggedInPublicModeUserCardContent(SystemTrayItem* owner) {
  user_card_view_->AddChildView(CreateIconForUserCard(user::LOGGED_IN_PUBLIC));
  user_card_view_->AddChildView(new PublicAccountUserDetails(
      owner, GetPreferredSize().width() + kTrayPopupPaddingBetweenItems));
}

void UserView::ToggleAddUserMenuOption() {
  if (add_menu_option_.get()) {
    popup_message_.reset();
    mouse_watcher_.reset();
    add_menu_option_.reset();
    return;
  }

  // Note: We do not need to install a global event handler to delete this
  // item since it will destroyed automatically before the menu / user menu item
  // gets destroyed..
  const SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();
  add_user_visible_but_disabled_ =
      session_state_delegate->NumberOfLoggedInUsers() >=
          session_state_delegate->GetMaximumNumberOfLoggedInUsers();
  add_menu_option_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_TOOLTIP;
  params.keep_on_top = true;
  params.context = this->GetWidget()->GetNativeWindow();
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  add_menu_option_->Init(params);
  add_menu_option_->SetOpacity(0xFF);
  add_menu_option_->GetNativeWindow()->set_owned_by_parent(false);
  SetShadowType(add_menu_option_->GetNativeView(),
                wm::SHADOW_TYPE_NONE);

  // Position it below our user card.
  gfx::Rect bounds = user_card_view_->GetBoundsInScreen();
  bounds.set_y(bounds.y() + bounds.height());
  add_menu_option_->SetBounds(bounds);

  // Show the content.
  AddUserView* add_user_view = new AddUserView(
      static_cast<UserCard*>(user_card_view_), this);
  add_menu_option_->SetContentsView(add_user_view);
  add_menu_option_->SetAlwaysOnTop(true);
  add_menu_option_->Show();
  if (add_user_visible_but_disabled_) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    popup_message_.reset(new PopupMessage(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_CAPTION_CANNOT_ADD_USER),
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_MESSAGE_CANNOT_ADD_USER),
        PopupMessage::ICON_WARNING,
        add_user_view->anchor(),
        views::BubbleBorder::TOP_LEFT,
        gfx::Size(parent()->bounds().width() - kPopupMessageOffset, 0),
        2 * kPopupMessageOffset));
  }
  // Find the screen area which encloses both elements and sets then a mouse
  // watcher which will close the "menu".
  gfx::Rect area = user_card_view_->GetBoundsInScreen();
  area.set_height(2 * area.height());
  mouse_watcher_.reset(new views::MouseWatcher(
      new UserViewMouseWatcherHost(area),
      this));
  mouse_watcher_->Start();
}

bool UserView::SupportsMultiProfile() {
  // We do not want to see any multi profile additions to a user view when the
  // log in screen is shown.
  return Shell::GetInstance()->delegate()->IsMultiProfilesEnabled() &&
      !Shell::GetInstance()->session_state_delegate()->IsUserSessionBlocked();
}

AddUserView::AddUserView(UserCard* owner, views::ButtonListener* listener)
    : CustomButton(listener),
      add_user_(NULL),
      listener_(listener),
      owner_(owner),
      anchor_(NULL) {
  AddContent();
  owner_->ForceBorderVisible(true);
}

AddUserView::~AddUserView() {
  owner_->ForceBorderVisible(false);
}

gfx::Size AddUserView::GetPreferredSize() {
  return owner_->bounds().size();
}

int AddUserView::GetHeightForWidth(int width) {
  return owner_->bounds().size().height();
}

void AddUserView::Layout() {
  gfx::Rect contents_area(GetContentsBounds());
  add_user_->SetBoundsRect(contents_area);
}

void AddUserView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (add_user_ == sender)
    listener_->ButtonPressed(this, event);
  else
    NOTREACHED();
}

void AddUserView::AddContent() {
  set_notify_enter_exit_on_child(true);

  const SessionStateDelegate* delegate =
      Shell::GetInstance()->session_state_delegate();
  bool enable = delegate->NumberOfLoggedInUsers() <
                    delegate->GetMaximumNumberOfLoggedInUsers();

  SetLayoutManager(new views::FillLayout());
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));

  // Add padding around the panel.
  SetBorder(views::Border::CreateSolidBorder(1, kBorderColor));

  add_user_ = new UserCard(this, enable);
  add_user_->SetBorder(views::Border::CreateEmptyBorder(
      kUserCardVerticalPadding,
      kTrayPopupPaddingHorizontal - kTrayUserTileHoverBorderInset,
      kUserCardVerticalPadding,
      kTrayPopupPaddingHorizontal - kTrayUserTileHoverBorderInset));

  add_user_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0 , kTrayPopupPaddingBetweenItems));
  AddChildViewAt(add_user_, 0);

  // Add the [+] icon which is also the anchor for messages.
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  RoundedImageView* icon = new RoundedImageView(kProfileRoundedCornerRadius,
                                                true);
  anchor_ = icon;
  icon->SetImage(*ui::ResourceBundle::GetSharedInstance().
      GetImageNamed(IDR_AURA_UBER_TRAY_ADD_MULTIPROFILE_USER).ToImageSkia(),
      gfx::Size(kUserIconSize, kUserIconSize));
  add_user_->AddChildView(icon);

  // Add the command text.
  views::Label* command_label = new views::Label(
      bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  command_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  add_user_->AddChildView(command_label);
}

}  // namespace tray

TrayUser::TrayUser(SystemTray* system_tray, MultiProfileIndex index)
    : SystemTrayItem(system_tray),
      multiprofile_index_(index),
      user_(NULL),
      layout_view_(NULL),
      avatar_(NULL),
      label_(NULL) {
  Shell::GetInstance()->system_tray_notifier()->AddUserObserver(this);
}

TrayUser::~TrayUser() {
  Shell::GetInstance()->system_tray_notifier()->RemoveUserObserver(this);
}

TrayUser::TestState TrayUser::GetStateForTest() const {
  if (!user_)
    return HIDDEN;
  return user_->GetStateForTest();
}

gfx::Size TrayUser::GetLayoutSizeForTest() const {
  if (!layout_view_) {
    return gfx::Size(0, 0);
  } else {
    return layout_view_->size();
  }
}

gfx::Rect TrayUser::GetUserPanelBoundsInScreenForTest() const {
  DCHECK(user_);
  return user_->GetBoundsInScreenOfUserButtonForTest();
}

void TrayUser::UpdateAfterLoginStatusChangeForTest(user::LoginStatus status) {
  UpdateAfterLoginStatusChange(status);
}

views::View* TrayUser::CreateTrayView(user::LoginStatus status) {
  CHECK(layout_view_ == NULL);

  layout_view_ = new views::View();
  layout_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           0, 0, kUserLabelToIconPadding));
  UpdateAfterLoginStatusChange(status);
  return layout_view_;
}

views::View* TrayUser::CreateDefaultView(user::LoginStatus status) {
  if (status == user::LOGGED_IN_NONE)
    return NULL;
  const SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();

  // If the screen is locked show only the currently active user.
  if (multiprofile_index_ && session_state_delegate->IsUserSessionBlocked())
    return NULL;

  CHECK(user_ == NULL);

  int logged_in_users = session_state_delegate->NumberOfLoggedInUsers();

  // Do not show more UserView's then there are logged in users.
  if (multiprofile_index_ >= logged_in_users)
    return NULL;

  user_ = new tray::UserView(this, status, multiprofile_index_);
  return user_;
}

views::View* TrayUser::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayUser::DestroyTrayView() {
  layout_view_ = NULL;
  avatar_ = NULL;
  label_ = NULL;
}

void TrayUser::DestroyDefaultView() {
  user_ = NULL;
}

void TrayUser::DestroyDetailedView() {
}

void TrayUser::UpdateAfterLoginStatusChange(user::LoginStatus status) {
  // Only the active user is represented in the tray.
  if (!layout_view_)
    return;
  if (GetTrayIndex() > 0)
    return;
  bool need_label = false;
  bool need_avatar = false;
  switch (status) {
    case user::LOGGED_IN_LOCKED:
    case user::LOGGED_IN_USER:
    case user::LOGGED_IN_OWNER:
    case user::LOGGED_IN_PUBLIC:
      need_avatar = true;
      break;
    case user::LOGGED_IN_LOCALLY_MANAGED:
      need_avatar = true;
      need_label = true;
      break;
    case user::LOGGED_IN_GUEST:
      need_label = true;
      break;
    case user::LOGGED_IN_RETAIL_MODE:
    case user::LOGGED_IN_KIOSK_APP:
    case user::LOGGED_IN_NONE:
      break;
  }

  if ((need_avatar != (avatar_ != NULL)) ||
      (need_label != (label_ != NULL))) {
    layout_view_->RemoveAllChildViews(true);
    if (need_label) {
      label_ = new views::Label;
      SetupLabelForTray(label_);
      layout_view_->AddChildView(label_);
    } else {
      label_ = NULL;
    }
    if (need_avatar) {
      avatar_ = new tray::RoundedImageView(kProfileRoundedCornerRadius, true);
      layout_view_->AddChildView(avatar_);
    } else {
      avatar_ = NULL;
    }
  }

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  if (status == user::LOGGED_IN_LOCALLY_MANAGED) {
    label_->SetText(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_LOCALLY_MANAGED_LABEL));
  } else if (status == user::LOGGED_IN_GUEST) {
    label_->SetText(bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_GUEST_LABEL));
  }

  if (avatar_ && switches::UseAlternateShelfLayout()) {
    avatar_->SetCornerRadii(
        0, kUserIconLargeCornerRadius, kUserIconLargeCornerRadius, 0);
    avatar_->SetBorder(views::Border::NullBorder());
  }
  UpdateAvatarImage(status);

  // Update layout after setting label_ and avatar_ with new login status.
  UpdateLayoutOfItem();
}

void TrayUser::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  // Inactive users won't have a layout.
  if (!layout_view_)
    return;
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_TOP) {
    if (avatar_) {
      if (switches::UseAlternateShelfLayout()) {
        avatar_->SetBorder(views::Border::NullBorder());
        avatar_->SetCornerRadii(
            0, kUserIconLargeCornerRadius, kUserIconLargeCornerRadius, 0);
      } else {
        avatar_->SetBorder(views::Border::CreateEmptyBorder(
            0,
            kTrayImageItemHorizontalPaddingBottomAlignment + 2,
            0,
            kTrayImageItemHorizontalPaddingBottomAlignment));
      }
    }
    if (label_) {
      // If label_ hasn't figured out its size yet, do that first.
      if (label_->GetContentsBounds().height() == 0)
        label_->SizeToPreferredSize();
      int height = label_->GetContentsBounds().height();
      int vertical_pad = (kTrayItemSize - height) / 2;
      int remainder = height % 2;
      label_->SetBorder(views::Border::CreateEmptyBorder(
          vertical_pad + remainder,
          kTrayLabelItemHorizontalPaddingBottomAlignment,
          vertical_pad,
          kTrayLabelItemHorizontalPaddingBottomAlignment));
    }
    layout_view_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal,
                             0, 0, kUserLabelToIconPadding));
  } else {
    if (avatar_) {
      if (switches::UseAlternateShelfLayout()) {
        avatar_->SetBorder(views::Border::NullBorder());
        avatar_->SetCornerRadii(
            0, 0, kUserIconLargeCornerRadius, kUserIconLargeCornerRadius);
      } else {
        SetTrayImageItemBorder(avatar_, alignment);
      }
    }
    if (label_) {
      label_->SetBorder(views::Border::CreateEmptyBorder(
          kTrayLabelItemVerticalPaddingVerticalAlignment,
          kTrayLabelItemHorizontalPaddingBottomAlignment,
          kTrayLabelItemVerticalPaddingVerticalAlignment,
          kTrayLabelItemHorizontalPaddingBottomAlignment));
    }
    layout_view_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical,
                             0, 0, kUserLabelToIconPadding));
  }
}

void TrayUser::OnUserUpdate() {
  UpdateAvatarImage(Shell::GetInstance()->system_tray_delegate()->
      GetUserLoginStatus());
}

void TrayUser::OnUserAddedToSession() {
  SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();
  // Only create views for user items which are logged in.
  if (GetTrayIndex() >= session_state_delegate->NumberOfLoggedInUsers())
    return;

  // Enforce a layout change that newly added items become visible.
  UpdateLayoutOfItem();

  // Update the user item.
  UpdateAvatarImage(
      Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus());
}

void TrayUser::UpdateAvatarImage(user::LoginStatus status) {
  SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();
  if (!avatar_ ||
      GetTrayIndex() >= session_state_delegate->NumberOfLoggedInUsers())
    return;

  int icon_size = switches::UseAlternateShelfLayout() ?
      kUserIconLargeSize : kUserIconSize;

  content::BrowserContext* context = session_state_delegate->
      GetBrowserContextByIndex(GetTrayIndex());
  avatar_->SetImage(session_state_delegate->GetUserImage(context),
                    gfx::Size(icon_size, icon_size));

  // Unit tests might come here with no images for some users.
  if (avatar_->size().IsEmpty())
    avatar_->SetSize(gfx::Size(icon_size, icon_size));
}

MultiProfileIndex TrayUser::GetTrayIndex() {
  Shell* shell = Shell::GetInstance();
  // If multi profile is not enabled we can use the normal index.
  if (!shell->delegate()->IsMultiProfilesEnabled())
    return multiprofile_index_;
  // In case of multi profile we need to mirror the indices since the system
  // tray items are in the reverse order then the menu items.
  return shell->session_state_delegate()->GetMaximumNumberOfLoggedInUsers() -
             1 - multiprofile_index_;
}

void TrayUser::UpdateLayoutOfItem() {
  RootWindowController* controller = GetRootWindowController(
      system_tray()->GetWidget()->GetNativeWindow()->GetRootWindow());
  if (controller && controller->shelf()) {
    UpdateAfterShelfAlignmentChange(
        controller->GetShelfLayoutManager()->GetAlignment());
  }
}

}  // namespace ash
