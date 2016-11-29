// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/user/user_card_view.h"

#include <algorithm>
#include <vector>

#include "ash/common/login_status.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/system/user/rounded_image_view.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user_info.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/compositing_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/border.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"

#if defined(OS_CHROMEOS)
#include "ash/common/ash_view_ids.h"
#include "ash/common/media_delegate.h"
#include "ash/common/system/chromeos/media_security/media_capture_observer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#endif

namespace ash {
namespace tray {

namespace {

const int kUserDetailsVerticalPadding = 5;

// The invisible word joiner character, used as a marker to indicate the start
// and end of the user's display name in the public account user card's text.
const base::char16 kDisplayNameMark[] = {0x2060, 0};

bool UseMd() {
  return MaterialDesignController::IsSystemTrayMenuMaterial();
}

views::View* CreateUserAvatarView(LoginStatus login_status, int user_index) {
  RoundedImageView* image_view = new RoundedImageView(
      UseMd() ? kTrayItemSize / 2 : kTrayRoundedBorderRadius, user_index == 0);
  if (login_status == LoginStatus::GUEST) {
    gfx::ImageSkia icon =
        UseMd() ? gfx::CreateVectorIcon(kSystemMenuGuestIcon, kMenuIconColor)
                : *ui::ResourceBundle::GetSharedInstance()
                       .GetImageNamed(IDR_AURA_UBER_TRAY_GUEST_ICON)
                       .ToImageSkia();
    image_view->SetImage(icon, icon.size());
  } else {
    SessionStateDelegate* delegate = WmShell::Get()->GetSessionStateDelegate();
    image_view->SetImage(delegate->GetUserInfo(user_index)->GetImage(),
                         gfx::Size(kTrayItemSize, kTrayItemSize));
  }

  if (UseMd()) {
    image_view->SetBorder(views::CreateEmptyBorder(gfx::Insets(
        (GetTrayConstant(TRAY_POPUP_ITEM_MAIN_IMAGE_CONTAINER_WIDTH) -
         image_view->GetPreferredSize().width()) /
        2)));
  }
  return image_view;
}

#if defined(OS_CHROMEOS)
class MediaIndicator : public views::View, public MediaCaptureObserver {
 public:
  explicit MediaIndicator(UserIndex index)
      : index_(index), label_(new views::Label) {
    SetLayoutManager(new views::FillLayout);
    views::ImageView* icon = new views::ImageView;
    icon->SetImage(ui::ResourceBundle::GetSharedInstance()
                       .GetImageNamed(IDR_AURA_UBER_TRAY_RECORDING_RED)
                       .ToImageSkia());
    AddChildView(icon);
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
        ui::ResourceBundle::SmallFont));
    OnMediaCaptureChanged();
    WmShell::Get()->system_tray_notifier()->AddMediaCaptureObserver(this);
    set_id(VIEW_ID_USER_VIEW_MEDIA_INDICATOR);
  }

  ~MediaIndicator() override {
    WmShell::Get()->system_tray_notifier()->RemoveMediaCaptureObserver(this);
  }

  // MediaCaptureObserver:
  void OnMediaCaptureChanged() override {
    MediaCaptureState state =
        WmShell::Get()->media_delegate()->GetMediaCaptureState(index_);
    int res_id = 0;
    switch (state) {
      case MEDIA_CAPTURE_AUDIO_VIDEO:
        res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_AUDIO_VIDEO;
        break;
      case MEDIA_CAPTURE_AUDIO:
        res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_AUDIO;
        break;
      case MEDIA_CAPTURE_VIDEO:
        res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_VIDEO;
        break;
      case MEDIA_CAPTURE_NONE:
        break;
    }
    SetMessage(res_id ? l10n_util::GetStringUTF16(res_id) : base::string16());
  }

  views::View* GetMessageView() { return label_; }

  void SetMessage(const base::string16& message) {
    SetVisible(!message.empty());
    label_->SetText(message);
    label_->SetVisible(!message.empty());
  }

 private:
  UserIndex index_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(MediaIndicator);
};
#endif

// The user details shown in public account mode. This is essentially a label
// but with custom painting code as the text is styled with multiple colors and
// contains a link.
class PublicAccountUserDetails : public views::View,
                                 public views::LinkListener {
 public:
  PublicAccountUserDetails(int max_width);
  ~PublicAccountUserDetails() override;

 private:
  // Overridden from views::View.
  void Layout() override;
  gfx::Size GetPreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // Overridden from views::LinkListener.
  void LinkClicked(views::Link* source, int event_flags) override;

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
  SetBorder(views::CreateEmptyBorder(
      kUserDetailsVerticalPadding, rtl ? 0 : inner_padding,
      kUserDetailsVerticalPadding, rtl ? inner_padding : 0));

  // Retrieve the user's display name and wrap it with markers.
  // Note that since this is a public account it always has to be the primary
  // user.
  base::string16 display_name = WmShell::Get()
                                    ->GetSessionStateDelegate()
                                    ->GetUserInfo(0)
                                    ->GetDisplayName();
  base::RemoveChars(display_name, kDisplayNameMark, &display_name);
  display_name = kDisplayNameMark[0] + display_name + kDisplayNameMark[0];
  // Retrieve the domain managing the device and wrap it with markers.
  base::string16 domain = base::UTF8ToUTF16(
      WmShell::Get()->system_tray_delegate()->GetEnterpriseDomain());
  base::RemoveChars(domain, kDisplayNameMark, &domain);
  base::i18n::WrapStringWithLTRFormatting(&domain);
  // Retrieve the label text, inserting the display name and domain.
  text_ = l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_PUBLIC_LABEL,
                                     display_name, domain);

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

gfx::Size PublicAccountUserDetails::GetPreferredSize() const {
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
  WmShell::Get()->system_tray_controller()->ShowPublicAccountInfo();
}

void PublicAccountUserDetails::CalculatePreferredSize(int max_allowed_width) {
  const gfx::FontList font_list;
  const gfx::Size link_size = learn_more_->GetPreferredSize();
  const int space_width =
      gfx::GetStringWidth(base::ASCIIToUTF16(" "), font_list);
  const gfx::Insets insets = GetInsets();
  int min_width = link_size.width();
  int max_width =
      gfx::GetStringWidth(text_, font_list) + space_width + link_size.width();
  // TODO(estade): |max_allowed_width| isn't used in MD.
  if (UseMd())
    DCHECK_EQ(-1, max_allowed_width);
  else
    max_width = std::min(max_width, max_allowed_width - insets.width());
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
  gfx::ElideRectangleText(text_, font_list, min_width, INT_MAX,
                          gfx::TRUNCATE_LONG_WORDS, &lines);
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

UserCardView::UserCardView(LoginStatus login_status,
                           int max_width,
                           int user_index)
    : user_index_(user_index) {
  auto layout = new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0,
                                     UseMd() ? kTrayPopupLabelHorizontalPadding
                                             : kTrayPopupPaddingBetweenItems);
  SetLayoutManager(layout);
  if (UseMd()) {
    layout->set_minimum_cross_axis_size(
        GetTrayConstant(TRAY_POPUP_ITEM_MIN_HEIGHT));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  }

  if (login_status == LoginStatus::PUBLIC) {
    AddPublicModeUserContent(max_width);
  } else {
    AddUserContent(login_status);
  }
}

UserCardView::~UserCardView() {}

void UserCardView::PaintChildren(const ui::PaintContext& context) {
  if (!is_active_user()) {
    ui::CompositingRecorder alpha(context, 0xFF / 2, true);
    View::PaintChildren(context);
  } else {
    View::PaintChildren(context);
  }
}

void UserCardView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_STATIC_TEXT;
  std::vector<base::string16> labels;
  for (int i = 0; i < child_count(); ++i)
    GetAccessibleLabelFromDescendantViews(child_at(i), labels);
  node_data->SetName(base::JoinString(labels, base::ASCIIToUTF16(" ")));
}

void UserCardView::AddPublicModeUserContent(int max_width) {
  views::View* avatar = CreateUserAvatarView(LoginStatus::PUBLIC, 0);
  AddChildView(avatar);
  int details_max_width = max_width - avatar->GetPreferredSize().width() -
                          kTrayPopupPaddingBetweenItems;
  AddChildView(new PublicAccountUserDetails(details_max_width));
}

void UserCardView::AddUserContent(LoginStatus login_status) {
  if (UseMd())
    return AddUserContentMd(login_status);
  views::View* avatar = CreateUserAvatarView(login_status, user_index_);
  AddChildView(avatar);
  views::Label* user_name = NULL;
  SessionStateDelegate* delegate = WmShell::Get()->GetSessionStateDelegate();
  if (!user_index_) {
    base::string16 user_name_string =
        login_status == LoginStatus::GUEST
            ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_GUEST_LABEL)
            : delegate->GetUserInfo(user_index_)->GetDisplayName();
    if (!user_name_string.empty()) {
      user_name = new views::Label(user_name_string);
      user_name->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
  }

  views::Label* user_email = NULL;
  if (login_status != LoginStatus::GUEST) {
    SystemTrayDelegate* tray_delegate = WmShell::Get()->system_tray_delegate();
    base::string16 user_email_string =
        tray_delegate->IsUserSupervised()
            ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL)
            : base::UTF8ToUTF16(
                  delegate->GetUserInfo(user_index_)->GetDisplayEmail());
    if (!user_email_string.empty()) {
      user_email = new views::Label(user_email_string);
      user_email->SetFontList(
          ui::ResourceBundle::GetSharedInstance().GetFontList(
              ui::ResourceBundle::SmallFont));
      user_email->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    }
  }

  // Adjust text properties dependent on if it is an active or inactive user.
  if (user_index_) {
    // Fade the text of non active users to 50%.
    SkColor text_color = user_email->enabled_color();
    text_color = SkColorSetA(text_color, SkColorGetA(text_color) / 2);
    if (user_email)
      user_email->SetDisabledColor(text_color);
    if (user_name)
      user_name->SetDisabledColor(text_color);
  }

  if (user_email && user_name) {
    views::View* details = new views::View;
    details->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, kUserDetailsVerticalPadding, 0));
    details->AddChildView(user_name);
    details->AddChildView(user_email);
    AddChildView(details);
  } else {
    if (user_name)
      AddChildView(user_name);
    if (user_email) {
#if defined(OS_CHROMEOS)
      // Only non active user can have a media indicator.
      MediaIndicator* media_indicator = new MediaIndicator(user_index_);
      views::View* email_indicator_view = new views::View;
      email_indicator_view->SetLayoutManager(new views::BoxLayout(
          views::BoxLayout::kHorizontal, 0, 0, kTrayPopupPaddingBetweenItems));
      email_indicator_view->AddChildView(user_email);
      email_indicator_view->AddChildView(media_indicator);

      views::View* details = new views::View;
      details->SetLayoutManager(new views::BoxLayout(
          views::BoxLayout::kVertical, 0, kUserDetailsVerticalPadding, 0));
      details->AddChildView(email_indicator_view);
      details->AddChildView(media_indicator->GetMessageView());
      AddChildView(details);
#else
      AddChildView(user_email);
#endif
    }
  }
}

void UserCardView::AddUserContentMd(LoginStatus login_status) {
  AddChildView(CreateUserAvatarView(login_status, user_index_));
  views::Label* user_name = nullptr;
  SessionStateDelegate* delegate = WmShell::Get()->GetSessionStateDelegate();
  if (is_active_user()) {
    base::string16 user_name_string =
        login_status == LoginStatus::GUEST
            ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_GUEST_LABEL)
            : delegate->GetUserInfo(user_index_)->GetDisplayName();
    if (!user_name_string.empty()) {
      user_name = new views::Label(user_name_string);
      user_name->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      TrayPopupItemStyle user_name_style(
          TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
      user_name_style.SetupLabel(user_name);
    }
  }

  views::Label* user_email = NULL;
  if (login_status != LoginStatus::GUEST) {
    SystemTrayDelegate* tray_delegate = WmShell::Get()->system_tray_delegate();
    base::string16 user_email_string =
        tray_delegate->IsUserSupervised()
            ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL)
            : base::UTF8ToUTF16(
                  delegate->GetUserInfo(user_index_)->GetDisplayEmail());
    if (!user_email_string.empty()) {
      user_email = new views::Label(user_email_string);
      user_email->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      TrayPopupItemStyle user_email_style(
          nullptr, TrayPopupItemStyle::FontStyle::CAPTION);
      user_email_style.set_color_style(
          TrayPopupItemStyle::ColorStyle::INACTIVE);
      user_email_style.SetupLabel(user_email);
    }
  }

  if (user_email && user_name) {
    views::View* details = new views::View;
    details->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
    details->AddChildView(user_name);
    details->AddChildView(user_email);
    // The name and email have different font sizes. This border is designed
    // to make both views take up equal space so the whitespace between them
    // is centered on the vertical midpoint.
    user_email->SetBorder(views::CreateEmptyBorder(
        0, 0, user_name->GetPreferredSize().height() -
                  user_email->GetPreferredSize().height(),
        0));
    AddChildView(details);
  } else if (user_name) {
    AddChildView(user_name);
  } else if (user_email) {
#if defined(OS_CHROMEOS)
    // Only non active user can have a media indicator.
    MediaIndicator* media_indicator = new MediaIndicator(user_index_);
    views::View* email_indicator_view = new views::View;
    email_indicator_view->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, kTrayPopupPaddingBetweenItems));
    email_indicator_view->AddChildView(user_email);
    email_indicator_view->AddChildView(media_indicator);

    views::View* details = new views::View;
    details->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, kUserDetailsVerticalPadding, 0));
    details->AddChildView(email_indicator_view);
    details->AddChildView(media_indicator->GetMessageView());
    AddChildView(details);
#else
    AddChildView(user_email);
#endif
  }
}

}  // namespace tray
}  // namespace ash
