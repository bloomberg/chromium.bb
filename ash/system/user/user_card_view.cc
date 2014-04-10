// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/user_card_view.h"

#include <algorithm>
#include <vector>

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/user/config.h"
#include "ash/system/user/rounded_image_view.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/size.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace tray {

namespace {

const int kUserDetailsVerticalPadding = 5;

// The invisible word joiner character, used as a marker to indicate the start
// and end of the user's display name in the public account user card's text.
const base::char16 kDisplayNameMark[] = {0x2060, 0};

// The user details shown in public account mode. This is essentially a label
// but with custom painting code as the text is styled with multiple colors and
// contains a link.
class PublicAccountUserDetails : public views::View,
                                 public views::LinkListener {
 public:
  PublicAccountUserDetails(int max_width);
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
  void CalculatePreferredSize(int max_allowed_width);

  base::string16 text_;
  views::Link* learn_more_;
  gfx::Size preferred_size_;
  ScopedVector<gfx::RenderText> lines_;

  DISALLOW_COPY_AND_ASSIGN(PublicAccountUserDetails);
};

PublicAccountUserDetails::PublicAccountUserDetails(int max_width)
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
  text_ = l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_PUBLIC_LABEL, display_name, domain);

  learn_more_ = new views::Link(l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE));
  learn_more_->SetUnderline(false);
  learn_more_->set_listener(this);
  AddChildView(learn_more_);

  CalculatePreferredSize(max_width);
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
  gfx::ElideRectangleText(text_,
                          font_list,
                          contents_area.width(),
                          contents_area.height(),
                          gfx::ELIDE_LONG_WORDS,
                          &lines);
  // Loop through the lines, creating a renderer for each.
  gfx::Point position = contents_area.origin();
  gfx::Range display_name(gfx::Range::InvalidRange());
  for (std::vector<base::string16>::const_iterator it = lines.begin();
       it != lines.end();
       ++it) {
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
       it != lines_.end();
       ++it) {
    (*it)->Draw(canvas);
  }
  views::View::OnPaint(canvas);
}

void PublicAccountUserDetails::LinkClicked(views::Link* source,
                                           int event_flags) {
  DCHECK_EQ(source, learn_more_);
  Shell::GetInstance()->system_tray_delegate()->ShowPublicAccountInfo();
}

void PublicAccountUserDetails::CalculatePreferredSize(int max_allowed_width) {
  const gfx::FontList font_list;
  const gfx::Size link_size = learn_more_->GetPreferredSize();
  const int space_width =
      gfx::GetStringWidth(base::ASCIIToUTF16(" "), font_list);
  const gfx::Insets insets = GetInsets();
  int min_width = link_size.width();
  int max_width = std::min(
      gfx::GetStringWidth(text_, font_list) + space_width + link_size.width(),
      max_allowed_width - insets.width());
  // Do a binary search for the minimum width that ensures no more than three
  // lines are needed. The lower bound is the minimum of the current bubble
  // width and the width of the link (as no wrapping is permitted inside the
  // link). The upper bound is the maximum of the largest allowed bubble width
  // and the sum of the label text and link widths when put on a single line.
  std::vector<base::string16> lines;
  while (min_width < max_width) {
    lines.clear();
    const int width = (min_width + max_width) / 2;
    const bool too_narrow = gfx::ElideRectangleText(text_,
                                                    font_list,
                                                    width,
                                                    INT_MAX,
                                                    gfx::TRUNCATE_LONG_WORDS,
                                                    &lines) != 0;
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
  preferred_size_ =
      gfx::Size(min_width + insets.width(),
                line_count * line_height + link_extra_height + insets.height());
}

}  // namespace

UserCardView::UserCardView(user::LoginStatus login_status,
                           int max_width,
                           int multiprofile_index) {
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kTrayPopupPaddingBetweenItems));
  switch (login_status) {
    case user::LOGGED_IN_RETAIL_MODE:
      AddRetailModeUserContent();
      break;
    case user::LOGGED_IN_PUBLIC:
      AddPublicModeUserContent(max_width);
      break;
    default:
      AddUserContent(login_status, multiprofile_index);
      break;
  }
}

UserCardView::~UserCardView() {}

void UserCardView::AddRetailModeUserContent() {
  views::Label* details = new views::Label;
  details->SetText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_KIOSK_LABEL));
  details->SetBorder(views::Border::CreateEmptyBorder(0, 4, 0, 1));
  details->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(details);
}

void UserCardView::AddPublicModeUserContent(int max_width) {
  views::View* icon = CreateIcon(user::LOGGED_IN_PUBLIC, 0);
  AddChildView(icon);
  int details_max_width = max_width - icon->GetPreferredSize().width() -
                          kTrayPopupPaddingBetweenItems;
  AddChildView(new PublicAccountUserDetails(details_max_width));
}

void UserCardView::AddUserContent(user::LoginStatus login_status,
                                  int multiprofile_index) {
  views::View* icon = CreateIcon(login_status, multiprofile_index);
  AddChildView(icon);
  views::Label* username = NULL;
  SessionStateDelegate* delegate =
      Shell::GetInstance()->session_state_delegate();
  if (!multiprofile_index) {
    base::string16 user_name_string =
        login_status == user::LOGGED_IN_GUEST
            ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_GUEST_LABEL)
            : delegate->GetUserDisplayName(multiprofile_index);
    if (user_name_string.empty() && IsMultiAccountSupportedAndUserActive())
      user_name_string =
          base::ASCIIToUTF16(delegate->GetUserEmail(multiprofile_index));
    if (!user_name_string.empty()) {
      username = new views::Label(user_name_string);
      username->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
  }

  views::Label* additional = NULL;
  if (login_status != user::LOGGED_IN_GUEST &&
      (multiprofile_index || !IsMultiAccountSupportedAndUserActive())) {
    base::string16 user_email_string =
        login_status == user::LOGGED_IN_LOCALLY_MANAGED
            ? l10n_util::GetStringUTF16(
                  IDS_ASH_STATUS_TRAY_LOCALLY_MANAGED_LABEL)
            : base::UTF8ToUTF16(delegate->GetUserEmail(multiprofile_index));
    if (!user_email_string.empty()) {
      additional = new views::Label(user_email_string);
      additional->SetFontList(
          ui::ResourceBundle::GetSharedInstance().GetFontList(
              ui::ResourceBundle::SmallFont));
      additional->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
  }

  // Adjust text properties dependent on if it is an active or inactive user.
  if (multiprofile_index) {
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
    AddChildView(details);
  } else {
    if (username)
      AddChildView(username);
    if (additional)
      AddChildView(additional);
  }
}

views::View* UserCardView::CreateIcon(user::LoginStatus login_status,
                                      int multiprofile_index) {
  RoundedImageView* icon =
      new RoundedImageView(kTrayAvatarCornerRadius, multiprofile_index == 0);
  if (login_status == user::LOGGED_IN_GUEST) {
    icon->SetImage(*ui::ResourceBundle::GetSharedInstance()
                        .GetImageNamed(IDR_AURA_UBER_TRAY_GUEST_ICON)
                        .ToImageSkia(),
                   gfx::Size(kTrayAvatarSize, kTrayAvatarSize));
  } else {
    SessionStateDelegate* delegate =
        Shell::GetInstance()->session_state_delegate();
    content::BrowserContext* context =
        delegate->GetBrowserContextByIndex(multiprofile_index);
    icon->SetImage(delegate->GetUserImage(context),
                   gfx::Size(kTrayAvatarSize, kTrayAvatarSize));
  }
  return icon;
}

}  // namespace tray
}  // namespace ash
