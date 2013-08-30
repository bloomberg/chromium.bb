// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

#include "ui/gfx/font.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"

#if defined(OS_WIN)
#include "win8/util/win8_util.h"
#endif  // OS_WIN

namespace {

// Besides the frame border, there's another 9 px of empty space atop the
// window in restored mode, to use to drag the window around.
const int kNonClientRestoredExtraThickness = 9;

// The titlebar never shrinks too short to show the caption button plus some
// padding below it.
const int kCaptionButtonHeightWithPadding = 19;

// There is a 5 px gap between the title text and the caption buttons.
const int kTitleLogoSpacing = 5;

// The frame border is only visible in restored mode and is hardcoded to 4 px on
// each side regardless of the system window border size.
const int kFrameBorderThickness = 4;

// The titlebar has a 2 px 3D edge along the top and bottom.
const int kTitlebarTopAndBottomEdgeThickness = 2;

// The icon is inset 2 px from the left frame border.
const int kIconLeftSpacing = 2;

// There is a 4 px gap between the icon and the title text.
const int kIconTitleSpacing = 4;

// The avatar ends 2 px above the bottom of the tabstrip (which, given the
// way the tabstrip draws its bottom edge, will appear like a 1 px gap to the
// user).
const int kAvatarBottomSpacing = 2;

// Space between the frame border and the left edge of the avatar.
const int kAvatarLeftSpacing = 2;

// Space between the right edge of the avatar and the tabstrip.
const int kAvatarRightSpacing = -2;

// In restored mode, the New Tab button isn't at the same height as the caption
// buttons, but the space will look cluttered if it actually slides under them,
// so we stop it when the gap between the two is down to 5 px.
const int kNewTabCaptionRestoredSpacing = 5;

// In maximized mode, where the New Tab button and the caption buttons are at
// similar vertical coordinates, we need to reserve a larger, 16 px gap to avoid
// looking too cluttered.
const int kNewTabCaptionMaximizedSpacing = 16;

// The top 3 px of the tabstrip is shadow; in maximized mode we push this off
// the top of the screen so the tabs appear flush against the screen edge.
const int kTabstripTopShadowThickness = 3;

// How far to indent the tabstrip from the left side of the screen when there
// is no avatar icon.
const int kTabStripIndent = -6;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, public:

OpaqueBrowserFrameViewLayout::OpaqueBrowserFrameViewLayout(
    OpaqueBrowserFrameViewLayoutDelegate* delegate)
    : delegate_(delegate),
      minimize_button_(NULL),
      maximize_button_(NULL),
      restore_button_(NULL),
      close_button_(NULL),
      window_icon_(NULL),
      window_title_(NULL),
      avatar_label_(NULL),
      avatar_button_(NULL) {
}

OpaqueBrowserFrameViewLayout::~OpaqueBrowserFrameViewLayout() {}

// static
bool OpaqueBrowserFrameViewLayout::ShouldAddDefaultCaptionButtons() {
#if defined(OS_WIN)
  return !win8::IsSingleWindowMetroMode();
#endif  // OS_WIN
  return true;
}

gfx::Rect OpaqueBrowserFrameViewLayout::GetBoundsForTabStrip(
    const gfx::Size& tabstrip_preferred_size,
    int available_width) const {
  gfx::Rect bounds = GetBoundsForTabStripAndAvatarArea(
      tabstrip_preferred_size, available_width);
  int space_left_of_tabstrip = kTabStripIndent;
  if (delegate_->ShouldShowAvatar()) {
    if (avatar_label_ && avatar_label_->bounds().width()) {
      // Space between the right edge of the avatar label and the tabstrip.
      const int kAvatarLabelRightSpacing = -10;
      space_left_of_tabstrip =
          avatar_label_->bounds().right() + kAvatarLabelRightSpacing;
    } else {
      space_left_of_tabstrip =
          kAvatarLeftSpacing + avatar_bounds_.width() +
          kAvatarRightSpacing;
    }
  }
  bounds.Inset(space_left_of_tabstrip, 0, 0, 0);
  return bounds;
}

gfx::Rect OpaqueBrowserFrameViewLayout::GetBoundsForTabStripAndAvatarArea(
    const gfx::Size& tabstrip_preferred_size,
    int available_width) const {
  if (minimize_button_) {
    available_width = minimize_button_->x();
  } else if (delegate_->GetAdditionalReservedSpaceInTabStrip()) {
    available_width -= delegate_->GetAdditionalReservedSpaceInTabStrip();
  }
  const int caption_spacing = delegate_->IsMaximized() ?
      kNewTabCaptionMaximizedSpacing : kNewTabCaptionRestoredSpacing;
  const int tabstrip_x = NonClientBorderThickness();
  const int tabstrip_width = available_width - tabstrip_x - caption_spacing;
  return gfx::Rect(tabstrip_x, GetTabStripInsetsTop(false),
                   std::max(0, tabstrip_width),
                   tabstrip_preferred_size.height());
}

gfx::Size OpaqueBrowserFrameViewLayout::GetMinimumSize(
    int available_width) const {
  gfx::Size min_size = delegate_->GetBrowserViewMinimumSize();
  int border_thickness = NonClientBorderThickness();
  min_size.Enlarge(2 * border_thickness,
                   NonClientTopBorderHeight(false) + border_thickness);

  int min_titlebar_width = (2 * FrameBorderThickness(false)) +
      kIconLeftSpacing +
      (delegate_->ShouldShowWindowIcon() ?
       (delegate_->GetIconSize() + kTitleLogoSpacing) : 0);
  if (ShouldAddDefaultCaptionButtons()) {
    min_titlebar_width +=
        minimize_button_->GetMinimumSize().width() +
        restore_button_->GetMinimumSize().width() +
        close_button_->GetMinimumSize().width();
  }
  min_size.set_width(std::max(min_size.width(), min_titlebar_width));

  // Ensure that the minimum width is enough to hold a minimum width tab strip
  // and avatar icon at their usual insets.
  if (delegate_->IsTabStripVisible()) {
    gfx::Size preferred_size = delegate_->GetTabstripPreferredSize();
    const int min_tabstrip_width = preferred_size.width();
    const int min_tabstrip_area_width =
        available_width -
        GetBoundsForTabStripAndAvatarArea(
            preferred_size, available_width).width() +
        min_tabstrip_width + delegate_->GetOTRAvatarIcon().width() +
        kAvatarLeftSpacing + kAvatarRightSpacing;
    min_size.set_width(std::max(min_size.width(), min_tabstrip_area_width));
  }

  return min_size;
}

gfx::Rect OpaqueBrowserFrameViewLayout::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight(false);
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

int OpaqueBrowserFrameViewLayout::FrameBorderThickness(bool restored) const {
  return (!restored && (delegate_->IsMaximized() ||
                        delegate_->IsFullscreen())) ?
      0 : kFrameBorderThickness;
}

int OpaqueBrowserFrameViewLayout::NonClientBorderThickness() const {
  // When we fill the screen, we don't show a client edge.
  return FrameBorderThickness(false) +
      ((delegate_->IsMaximized() || delegate_->IsFullscreen()) ?
       0 : views::NonClientFrameView::kClientEdgeThickness);
}

int OpaqueBrowserFrameViewLayout::NonClientTopBorderHeight(
    bool restored) const {
  if (delegate_->ShouldShowWindowTitle()) {
    return std::max(FrameBorderThickness(restored) + delegate_->GetIconSize(),
        CaptionButtonY(restored) + kCaptionButtonHeightWithPadding) +
        TitlebarBottomThickness(restored);
  }

  return FrameBorderThickness(restored) -
      ((delegate_->IsTabStripVisible() &&
          !restored && !delegate_->ShouldLeaveOffsetNearTopBorder())
              ? kTabstripTopShadowThickness : 0);
}

int OpaqueBrowserFrameViewLayout::GetTabStripInsetsTop(bool restored) const {
  return NonClientTopBorderHeight(restored) + ((!restored &&
      (!delegate_->ShouldLeaveOffsetNearTopBorder() ||
      delegate_->IsFullscreen())) ?
      0 : kNonClientRestoredExtraThickness);
}

int OpaqueBrowserFrameViewLayout::TitlebarBottomThickness(bool restored) const {
  return kTitlebarTopAndBottomEdgeThickness +
      ((!restored && delegate_->IsMaximized()) ? 0 :
       views::NonClientFrameView::kClientEdgeThickness);
}

int OpaqueBrowserFrameViewLayout::CaptionButtonY(bool restored) const {
  // Maximized buttons start at window top so that even if their images aren't
  // drawn flush with the screen edge, they still obey Fitts' Law.
  return (!restored && delegate_->IsMaximized()) ?
      FrameBorderThickness(false) :
      views::NonClientFrameView::kFrameShadowThickness;
}

gfx::Rect OpaqueBrowserFrameViewLayout::IconBounds() const {
  int size = delegate_->GetIconSize();
  int frame_thickness = FrameBorderThickness(false);
  int y;
  if (delegate_->ShouldShowWindowIcon() ||
      delegate_->ShouldShowWindowTitle()) {
    // Our frame border has a different "3D look" than Windows'.  Theirs has a
    // more complex gradient on the top that they push their icon/title below;
    // then the maximized window cuts this off and the icon/title are centered
    // in the remaining space.  Because the apparent shape of our border is
    // simpler, using the same positioning makes things look slightly uncentered
    // with restored windows, so when the window is restored, instead of
    // calculating the remaining space from below the frame border, we calculate
    // from below the 3D edge.
    int unavailable_px_at_top = delegate_->IsMaximized() ?
        frame_thickness : kTitlebarTopAndBottomEdgeThickness;
    // When the icon is shorter than the minimum space we reserve for the
    // caption button, we vertically center it.  We want to bias rounding to put
    // extra space above the icon, since the 3D edge (+ client edge, for
    // restored windows) below looks (to the eye) more like additional space
    // than does the 3D edge (or nothing at all, for maximized windows) above;
    // hence the +1.
    y = unavailable_px_at_top + (NonClientTopBorderHeight(false) -
        unavailable_px_at_top - size - TitlebarBottomThickness(false) + 1) / 2;
  } else {
    // For "browser mode" windows, we use the native positioning, which is just
    // below the top frame border.
    y = frame_thickness;
  }
  return gfx::Rect(frame_thickness + kIconLeftSpacing, y, size, size);
}

gfx::Rect OpaqueBrowserFrameViewLayout::CalculateClientAreaBounds(
    int width,
    int height) const {
  int top_height = NonClientTopBorderHeight(false);
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, private:

void OpaqueBrowserFrameViewLayout::LayoutWindowControls(views::View* host) {
  if (!ShouldAddDefaultCaptionButtons())
    return;
  bool is_maximized = delegate_->IsMaximized();
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                   views::ImageButton::ALIGN_BOTTOM);
  int caption_y = CaptionButtonY(false);
  // There should always be the same number of non-shadow pixels visible to the
  // side of the caption buttons.  In maximized mode we extend the rightmost
  // button to the screen corner to obey Fitts' Law.
  int right_extra_width = is_maximized ?
      (kFrameBorderThickness -
       views::NonClientFrameView::kFrameShadowThickness) : 0;
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(host->width() - FrameBorderThickness(false) -
      right_extra_width - close_button_size.width(), caption_y,
      close_button_size.width() + right_extra_width,
      close_button_size.height());

  // When the window is restored, we show a maximized button; otherwise, we show
  // a restore button.
  bool is_restored = !is_maximized && !delegate_->IsMinimized();
  views::ImageButton* invisible_button = is_restored ?
      restore_button_ : maximize_button_;
  invisible_button->SetVisible(false);

  views::ImageButton* visible_button = is_restored ?
      maximize_button_ : restore_button_;
  visible_button->SetVisible(true);
  visible_button->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                    views::ImageButton::ALIGN_BOTTOM);
  gfx::Size visible_button_size = visible_button->GetPreferredSize();
  visible_button->SetBounds(close_button_->x() - visible_button_size.width(),
                            caption_y, visible_button_size.width(),
                            visible_button_size.height());

  minimize_button_->SetVisible(true);
  minimize_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                      views::ImageButton::ALIGN_BOTTOM);
  gfx::Size minimize_button_size = minimize_button_->GetPreferredSize();
  minimize_button_->SetBounds(
      visible_button->x() - minimize_button_size.width(), caption_y,
      minimize_button_size.width(),
      minimize_button_size.height());
}

void OpaqueBrowserFrameViewLayout::LayoutTitleBar() {
  gfx::Rect icon_bounds(IconBounds());
  if (delegate_->ShouldShowWindowIcon() && window_icon_)
    window_icon_->SetBoundsRect(icon_bounds);

  if (window_title_) {
    bool should_show = delegate_->ShouldShowWindowTitle();
    window_title_->SetVisible(should_show);

    if (should_show) {
      window_title_->SetText(delegate_->GetWindowTitle());
      const int title_x = delegate_->ShouldShowWindowIcon() ?
          icon_bounds.right() + kIconTitleSpacing : icon_bounds.x();
      window_title_->SetBounds(title_x, icon_bounds.y(),
          std::max(0, minimize_button_->x() - kTitleLogoSpacing - title_x),
          icon_bounds.height());
    }
  }
}

void OpaqueBrowserFrameViewLayout::LayoutAvatar() {
  // Even though the avatar is used for both incognito and profiles we always
  // use the incognito icon to layout the avatar button. The profile icon
  // can be customized so we can't depend on its size to perform layout.
  gfx::ImageSkia incognito_icon = delegate_->GetOTRAvatarIcon();

  int avatar_bottom = GetTabStripInsetsTop(false) +
      delegate_->GetTabStripHeight() - kAvatarBottomSpacing;
  int avatar_restored_y = avatar_bottom - incognito_icon.height();
  int avatar_y = delegate_->IsMaximized() ?
      (NonClientTopBorderHeight(false) + kTabstripTopShadowThickness) :
      avatar_restored_y;
  avatar_bounds_.SetRect(NonClientBorderThickness() + kAvatarLeftSpacing,
      avatar_y, incognito_icon.width(),
      delegate_->ShouldShowAvatar() ? (avatar_bottom - avatar_y) : 0);
  if (avatar_button_)
    avatar_button_->SetBoundsRect(avatar_bounds_);

  if (avatar_label_) {
    // Space between the bottom of the avatar and the bottom of the avatar
    // label.
    const int kAvatarLabelBottomSpacing = 3;
    // Space between the frame border and the left edge of the avatar label.
    const int kAvatarLabelLeftSpacing = -1;
    gfx::Size label_size = avatar_label_->GetPreferredSize();
    gfx::Rect label_bounds(
        FrameBorderThickness(false) + kAvatarLabelLeftSpacing,
        avatar_bottom - kAvatarLabelBottomSpacing - label_size.height(),
        label_size.width(),
        delegate_->ShouldShowAvatar() ? label_size.height() : 0);
    avatar_label_->SetBoundsRect(label_bounds);
  }
}

void OpaqueBrowserFrameViewLayout::SetView(int id, views::View* view) {
  // Why do things this way instead of having an Init() method, where we're
  // passed the views we'll handle? Because OpaqueBrowserFrameView doesn't own
  // all the views which are part of it. The avatar stuff, for example, will be
  // added and removed by the base class of OpaqueBrowserFrameView.
  switch (id) {
    case VIEW_ID_MINIMIZE_BUTTON:
      if (view) {
        DCHECK_EQ(std::string(views::ImageButton::kViewClassName),
                  view->GetClassName());
      }
      minimize_button_ = static_cast<views::ImageButton*>(view);
      break;
    case VIEW_ID_MAXIMIZE_BUTTON:
      if (view) {
        DCHECK_EQ(std::string(views::ImageButton::kViewClassName),
                  view->GetClassName());
      }
      maximize_button_ = static_cast<views::ImageButton*>(view);
      break;
    case VIEW_ID_RESTORE_BUTTON:
      if (view) {
        DCHECK_EQ(std::string(views::ImageButton::kViewClassName),
                  view->GetClassName());
      }
      restore_button_ = static_cast<views::ImageButton*>(view);
      break;
    case VIEW_ID_CLOSE_BUTTON:
      if (view) {
        DCHECK_EQ(std::string(views::ImageButton::kViewClassName),
                  view->GetClassName());
      }
      close_button_ = static_cast<views::ImageButton*>(view);
      break;
    case VIEW_ID_WINDOW_ICON:
      window_icon_ = view;
      break;
    case VIEW_ID_WINDOW_TITLE:
      if (view) {
        DCHECK_EQ(std::string(views::Label::kViewClassName),
                  view->GetClassName());
      }
      window_title_ = static_cast<views::Label*>(view);
      break;
    case VIEW_ID_AVATAR_LABEL:
      avatar_label_ = view;
      break;
    case VIEW_ID_AVATAR_BUTTON:
      avatar_button_ = view;
      break;
    default:
      NOTIMPLEMENTED() << "Unknown view id " << id;
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameView, views::LayoutManager:

void OpaqueBrowserFrameViewLayout::Layout(views::View* host) {
  LayoutWindowControls(host);
  LayoutTitleBar();
  LayoutAvatar();

  client_view_bounds_ = CalculateClientAreaBounds(
      host->width(), host->height());
}

gfx::Size OpaqueBrowserFrameViewLayout::GetPreferredSize(views::View* host) {
  // This is never used; NonClientView::GetPreferredSize() will be called
  // instead.
  NOTREACHED();
  return gfx::Size();
}

void OpaqueBrowserFrameViewLayout::ViewAdded(views::View* host,
                                             views::View* view) {
  SetView(view->id(), view);
}

void OpaqueBrowserFrameViewLayout::ViewRemoved(views::View* host,
                                               views::View* view) {
  SetView(view->id(), NULL);
}
