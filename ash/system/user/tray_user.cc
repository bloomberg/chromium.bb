// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/tray_user.h"

#include <algorithm>
#include <climits>
#include <vector>

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/range/range.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

const int kUserDetailsVerticalPadding = 5;
const int kUserCardVerticalPadding = 10;
const int kProfileRoundedCornerRadius = 2;
const int kUserIconSize = 27;

// The invisible word joiner character, used as a marker to indicate the start
// and end of the user's display name in the public account user card's text.
const char16 kDisplayNameMark[] = { 0x2060, 0 };

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

}  // namespace

namespace ash {
namespace internal {

namespace tray {

// A custom image view with rounded edges.
class RoundedImageView : public views::View {
 public:
  // Constructs a new rounded image view with rounded corners of radius
  // |corner_radius|.
  explicit RoundedImageView(int corner_radius);
  virtual ~RoundedImageView();

  // Set the image that should be displayed. The image contents is copied to the
  // receiver's image.
  void SetImage(const gfx::ImageSkia& img, const gfx::Size& size);

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  gfx::ImageSkia image_;
  gfx::ImageSkia resized_;
  gfx::Size image_size_;
  int corner_radius_;

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

  string16 text_;
  views::Link* learn_more_;
  gfx::Size preferred_size_;
  ScopedVector<gfx::RenderText> lines_;

  DISALLOW_COPY_AND_ASSIGN(PublicAccountUserDetails);
};

class UserView : public views::View,
                 public views::ButtonListener {
 public:
  explicit UserView(SystemTrayItem* owner, ash::user::LoginStatus login);
  virtual ~UserView();

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  void AddLogoutButton(ash::user::LoginStatus login);
  void AddUserCard(SystemTrayItem* owner, ash::user::LoginStatus login);

  views::View* user_card_;
  views::View* logout_button_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

RoundedImageView::RoundedImageView(int corner_radius)
    : corner_radius_(corner_radius) {}

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

gfx::Size RoundedImageView::GetPreferredSize() {
  return gfx::Size(image_size_.width() + GetInsets().width(),
                   image_size_.height() + GetInsets().height());
}

void RoundedImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  gfx::Rect image_bounds(size());
  image_bounds.ClampToCenteredSize(GetPreferredSize());
  image_bounds.Inset(GetInsets());
  const SkScalar kRadius = SkIntToScalar(corner_radius_);
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(image_bounds), kRadius, kRadius);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  canvas->DrawImageInPath(resized_, image_bounds.x(), image_bounds.y(),
                          path, paint);
}

PublicAccountUserDetails::PublicAccountUserDetails(SystemTrayItem* owner,
                                                   int used_width)
    : learn_more_(NULL) {
  const int inner_padding =
      kTrayPopupPaddingHorizontal - kTrayPopupPaddingBetweenItems;
  const bool rtl = base::i18n::IsRTL();
  set_border(views::Border::CreateEmptyBorder(
      kUserDetailsVerticalPadding, rtl ? 0 : inner_padding,
      kUserDetailsVerticalPadding, rtl ? inner_padding : 0));

  ash::SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->system_tray_delegate();
  // Retrieve the user's display name and wrap it with markers.
  string16 display_name = delegate->GetUserDisplayName();
  RemoveChars(display_name, kDisplayNameMark, &display_name);
  display_name = kDisplayNameMark[0] + display_name + kDisplayNameMark[0];
  // Retrieve the domain managing the device and wrap it with markers.
  string16 domain = UTF8ToUTF16(delegate->GetEnterpriseDomain());
  RemoveChars(domain, kDisplayNameMark, &domain);
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
  const gfx::Font font;
  std::vector<string16> lines;
  ui::ElideRectangleText(text_, font, contents_area.width(),
                         contents_area.height(), ui::ELIDE_LONG_WORDS, &lines);
  // Loop through the lines, creating a renderer for each.
  gfx::Point position = contents_area.origin();
  ui::Range display_name(ui::Range::InvalidRange());
  for (std::vector<string16>::const_iterator it = lines.begin();
       it != lines.end(); ++it) {
    gfx::RenderText* line = gfx::RenderText::CreateInstance();
    line->SetDirectionalityMode(gfx::DIRECTIONALITY_FROM_UI);
    line->SetText(*it);
    const gfx::Size size(contents_area.width(), line->GetStringSize().height());
    line->SetDisplayRect(gfx::Rect(position, size));
    position.set_y(position.y() + size.height());

    // Set the default text color for the line.
    gfx::StyleRange default_style(line->default_style());
    default_style.foreground = kPublicAccountUserCardTextColor;
    line->set_default_style(default_style);
    line->ApplyDefaultStyle();

    // If a range of the line contains the user's display name, apply a custom
    // text color to it.
    if (display_name.is_empty())
      display_name.set_start(it->find(kDisplayNameMark));
    if (!display_name.is_empty()) {
      display_name.set_end(
          it->find(kDisplayNameMark, display_name.start() + 1));
      gfx::StyleRange display_name_style(line->default_style());
      display_name_style.foreground = kPublicAccountUserCardNameColor;
      ui::Range line_range(0, it->size());
      display_name_style.range = display_name.Intersect(line_range);
      line->ApplyStyleRange(display_name_style);
      // Update the range for the next line.
      if (display_name.end() >= line_range.end())
        display_name.set_start(0);
      else
        display_name = ui::Range::InvalidRange();
    }

    lines_.push_back(line);
  }

  // Position the link after the label text, separated by a space. If it does
  // not fit onto the last line of the text, wrap the link onto its own line.
  const gfx::Size last_line_size = lines_.back()->GetStringSize();
  const int space_width = font.GetStringWidth(ASCIIToUTF16(" "));
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
  ash::Shell::GetInstance()->system_tray_delegate()->ShowPublicAccountInfo();
}

void PublicAccountUserDetails::CalculatePreferredSize(SystemTrayItem* owner,
                                                      int used_width) {
  const gfx::Font font;
  const gfx::Size link_size = learn_more_->GetPreferredSize();
  const int space_width = font.GetStringWidth(ASCIIToUTF16(" "));
  const gfx::Insets insets = GetInsets();
  views::TrayBubbleView* bubble_view =
      owner->system_tray()->GetSystemBubble()->bubble_view();
  int min_width = std::max(
      link_size.width(),
      bubble_view->GetPreferredSize().width() - (used_width + insets.width()));
  int max_width = std::min(
      font.GetStringWidth(text_) + space_width + link_size.width(),
      bubble_view->GetMaximumSize().width() - (used_width + insets.width()));
  // Do a binary search for the minimum width that ensures no more than three
  // lines are needed. The lower bound is the minimum of the current bubble
  // width and the width of the link (as no wrapping is permitted inside the
  // link). The upper bound is the maximum of the largest allowed bubble width
  // and the sum of the label text and link widths when put on a single line.
  std::vector<string16> lines;
  while (min_width < max_width) {
    lines.clear();
    const int width = (min_width + max_width) / 2;
    const bool too_narrow = ui::ElideRectangleText(
        text_, font, width, INT_MAX, ui::TRUNCATE_LONG_WORDS, &lines) != 0;
    int line_count = lines.size();
    if (!too_narrow && line_count == 3 &&
            width - font.GetStringWidth(lines.back()) <=
            space_width + link_size.width()) {
      ++line_count;
    }
    if (too_narrow || line_count > 3)
      min_width = width + 1;
    else
      max_width = width;
  }

  // Calculate the corresponding height and set the preferred size.
  lines.clear();
  ui::ElideRectangleText(
      text_, font, min_width, INT_MAX, ui::TRUNCATE_LONG_WORDS, &lines);
  int line_count = lines.size();
  if (min_width - font.GetStringWidth(lines.back()) <=
      space_width + link_size.width()) {
    ++line_count;
  }
  const int line_height = font.GetHeight();
  const int link_extra_height = std::max(
      link_size.height() - learn_more_->GetInsets().top() - line_height, 0);
  preferred_size_ = gfx::Size(
      min_width + insets.width(),
      line_count * line_height + link_extra_height + insets.height());

  bubble_view->SetWidth(preferred_size_.width() + used_width);
}

UserView::UserView(SystemTrayItem* owner, ash::user::LoginStatus login)
    : user_card_(NULL),
      logout_button_(NULL) {
  CHECK_NE(ash::user::LOGGED_IN_NONE, login);
  set_background(views::Background::CreateSolidBackground(
      login == ash::user::LOGGED_IN_PUBLIC ? kPublicAccountBackgroundColor :
                                             kBackgroundColor));
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                                        kTrayPopupPaddingBetweenItems));
  // The logout button must be added before the user card so that the user card
  // can correctly calculate the remaining available width.
  AddLogoutButton(login);
  AddUserCard(owner, login);
}

UserView::~UserView() {}

gfx::Size UserView::GetPreferredSize() {
  gfx::Size size = views::View::GetPreferredSize();
  if (!user_card_) {
    // Make sure the default user view item is at least as tall as the other
    // items.
    size.set_height(std::max(size.height(),
                             kTrayPopupItemHeight + GetInsets().height()));
  }
  return size;
}

void UserView::Layout() {
  gfx::Rect contents_area(GetContentsBounds());
  if (user_card_ && logout_button_) {
    // Give the logout button the space it requests.
    gfx::Rect logout_area = contents_area;
    logout_area.ClampToCenteredSize(logout_button_->GetPreferredSize());
    logout_area.set_x(contents_area.right() - logout_area.width());
    logout_button_->SetBoundsRect(logout_area);

    // Give the remaining space to the user card.
    gfx::Rect user_card_area = contents_area;
    user_card_area.set_width(contents_area.width() -
        (logout_area.width() + kTrayPopupPaddingBetweenItems));
    user_card_->SetBoundsRect(user_card_area);
  } else if (user_card_) {
    user_card_->SetBoundsRect(contents_area);
  } else if (logout_button_) {
    logout_button_->SetBoundsRect(contents_area);
  }
}

void UserView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  DCHECK_EQ(logout_button_, sender);
  ash::Shell::GetInstance()->system_tray_delegate()->SignOut();
}

void UserView::AddLogoutButton(ash::user::LoginStatus login) {
  // A user should not be able to modify logged-in state when screen is
  // locked.
  if (login == ash::user::LOGGED_IN_LOCKED)
    return;

  const string16 title = ash::user::GetLocalizedSignOutStringForStatus(login,
                                                                       true);
  TrayPopupLabelButton* logout_button = new TrayPopupLabelButton(this, title);
  logout_button->SetAccessibleName(title);
  logout_button_ = logout_button;
  // In public account mode, the logout button border has a custom color.
  if (login == ash::user::LOGGED_IN_PUBLIC) {
    TrayPopupLabelButtonBorder* border =
        static_cast<TrayPopupLabelButtonBorder*>(logout_button_->border());
    border->SetPainter(views::CustomButton::STATE_NORMAL,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesNormal));
    border->SetPainter(views::CustomButton::STATE_HOVERED,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesHovered));
    border->SetPainter(views::CustomButton::STATE_PRESSED,
                       views::Painter::CreateImageGridPainter(
                           kPublicAccountLogoutButtonBorderImagesHovered));
  }
  AddChildView(logout_button_);
}

void UserView::AddUserCard(SystemTrayItem* owner,
                           ash::user::LoginStatus login) {
  if (login == ash::user::LOGGED_IN_GUEST)
    return;

  set_border(views::Border::CreateEmptyBorder(0, kTrayPopupPaddingHorizontal,
                                              0, kTrayPopupPaddingHorizontal));

  user_card_ = new views::View();
  user_card_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, kUserCardVerticalPadding,
      kTrayPopupPaddingBetweenItems));
  AddChildViewAt(user_card_, 0);

  if (login == ash::user::LOGGED_IN_KIOSK) {
    views::Label* details = new views::Label;
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    details->SetText(
        bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_KIOSK_LABEL));
    details->set_border(views::Border::CreateEmptyBorder(0, 4, 0, 1));
    details->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    user_card_->AddChildView(details);
    return;
  }

  RoundedImageView* avatar = new RoundedImageView(kProfileRoundedCornerRadius);
  avatar->SetImage(
      ash::Shell::GetInstance()->system_tray_delegate()->GetUserImage(),
      gfx::Size(kUserIconSize, kUserIconSize));
  user_card_->AddChildView(avatar);

  if (login == ash::user::LOGGED_IN_PUBLIC) {
    user_card_->AddChildView(new PublicAccountUserDetails(
        owner, GetPreferredSize().width() + kTrayPopupPaddingBetweenItems));
    return;
  }

  ash::SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->system_tray_delegate();
  views::View* details = new views::View;
  details->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, kUserDetailsVerticalPadding, 0));
  views::Label* username = new views::Label(delegate->GetUserDisplayName());
  username->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  details->AddChildView(username);

  views::Label* email = new views::Label(UTF8ToUTF16(delegate->GetUserEmail()));
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  email->SetFont(bundle.GetFont(ui::ResourceBundle::SmallFont));
  email->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  email->SetEnabled(false);
  details->AddChildView(email);
  user_card_->AddChildView(details);
}

}  // namespace tray

TrayUser::TrayUser(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      user_(NULL),
      avatar_(NULL),
      label_(NULL) {
  Shell::GetInstance()->system_tray_notifier()->AddUserObserver(this);
}

TrayUser::~TrayUser() {
  Shell::GetInstance()->system_tray_notifier()->RemoveUserObserver(this);
}

views::View* TrayUser::CreateTrayView(user::LoginStatus status) {
  CHECK(avatar_ == NULL);
  CHECK(label_ == NULL);
  if (status == user::LOGGED_IN_GUEST) {
    label_ = new views::Label;
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    label_->SetText(bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_GUEST_LABEL));
    SetupLabelForTray(label_);
  } else {
    avatar_ = new tray::RoundedImageView(kProfileRoundedCornerRadius);
  }
  UpdateAfterLoginStatusChange(status);
  return avatar_ ? static_cast<views::View*>(avatar_)
                 : static_cast<views::View*>(label_);
}

views::View* TrayUser::CreateDefaultView(user::LoginStatus status) {
  if (status == user::LOGGED_IN_NONE)
    return NULL;

  CHECK(user_ == NULL);
  user_ = new tray::UserView(this, status);
  return user_;
}

views::View* TrayUser::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayUser::DestroyTrayView() {
  avatar_ = NULL;
  label_ = NULL;
}

void TrayUser::DestroyDefaultView() {
  user_ = NULL;
}

void TrayUser::DestroyDetailedView() {
}

void TrayUser::UpdateAfterLoginStatusChange(user::LoginStatus status) {
  switch (status) {
    case user::LOGGED_IN_LOCKED:
    case user::LOGGED_IN_USER:
    case user::LOGGED_IN_OWNER:
    case user::LOGGED_IN_PUBLIC:
    case user::LOGGED_IN_LOCALLY_MANAGED:
      avatar_->SetImage(
          ash::Shell::GetInstance()->system_tray_delegate()->GetUserImage(),
          gfx::Size(kUserIconSize, kUserIconSize));
      avatar_->SetVisible(true);
      break;

    case user::LOGGED_IN_GUEST:
      label_->SetVisible(true);
      break;

    case user::LOGGED_IN_KIOSK:
    case user::LOGGED_IN_NONE:
      avatar_->SetVisible(false);
      break;
  }
}

void TrayUser::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  if (avatar_) {
    if (alignment == SHELF_ALIGNMENT_BOTTOM ||
        alignment == SHELF_ALIGNMENT_TOP) {
      avatar_->set_border(views::Border::CreateEmptyBorder(
          0, kTrayImageItemHorizontalPaddingBottomAlignment + 2,
          0, kTrayImageItemHorizontalPaddingBottomAlignment));
    } else {
        SetTrayImageItemBorder(avatar_, alignment);
    }
  } else {
    if (alignment == SHELF_ALIGNMENT_BOTTOM ||
        alignment == SHELF_ALIGNMENT_TOP) {
      label_->set_border(views::Border::CreateEmptyBorder(
          0, kTrayLabelItemHorizontalPaddingBottomAlignment,
          0, kTrayLabelItemHorizontalPaddingBottomAlignment));
    } else {
      label_->set_border(views::Border::CreateEmptyBorder(
          kTrayLabelItemVerticalPaddingVeriticalAlignment,
          kTrayLabelItemHorizontalPaddingBottomAlignment,
          kTrayLabelItemVerticalPaddingVeriticalAlignment,
          kTrayLabelItemHorizontalPaddingBottomAlignment));
    }
  }
}

void TrayUser::OnUserUpdate() {
  // Check for null to avoid crbug.com/150944.
  if (avatar_) {
    avatar_->SetImage(
        ash::Shell::GetInstance()->system_tray_delegate()->GetUserImage(),
        gfx::Size(kUserIconSize, kUserIconSize));
  }
}

}  // namespace internal
}  // namespace ash
