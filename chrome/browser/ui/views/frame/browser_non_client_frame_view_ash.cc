// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include "ash/ash_switches.h"
#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "ash/wm/frame_painter.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/immersive_fullscreen_configuration.h"
#include "chrome/browser/ui/views/avatar_label.h"
#include "chrome/browser/ui/views/avatar_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "content/public/browser/web_contents.h"
#include "grit/ash_resources.h"
#include "grit/theme_resources.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// The avatar ends 2 px above the bottom of the tabstrip (which, given the
// way the tabstrip draws its bottom edge, will appear like a 1 px gap to the
// user).
const int kAvatarBottomSpacing = 2;
// There are 2 px on each side of the avatar (between the frame border and
// it on the left, and between it and the tabstrip on the right).
const int kAvatarSideSpacing = 2;
// Space between left edge of window and tabstrip.
const int kTabstripLeftSpacing = 0;
// Space between right edge of tabstrip and maximize button.
const int kTabstripRightSpacing = 10;
// Height of the shadow of the content area, at the top of the toolbar.
const int kContentShadowHeight = 1;
// Space between top of window and top of tabstrip for tall headers, such as
// for restored windows, apps, etc.
const int kTabstripTopSpacingTall = 7;
// Space between top of window and top of tabstrip for short headers, such as
// for maximized windows, pop-ups, etc.
const int kTabstripTopSpacingShort = 0;
// Height of the shadow in the tab image, used to ensure clicks in the shadow
// area still drag restored windows.  This keeps the clickable area large enough
// to hit easily.
const int kTabShadowHeight = 4;

// Space between right edge of tabstrip and caption buttons when using the
// alternate caption button style.
const int kTabstripRightSpacingAlternateCaptionButtonStyle = 0;
// Space between top of window and top of tabstrip for short headers when using
// the alternate caption button style.
const int kTabstripTopSpacingShortAlternateCaptionButtonStyle = 4;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, public:

// static
const char BrowserNonClientFrameViewAsh::kViewClassName[] =
    "BrowserNonClientFrameViewAsh";

BrowserNonClientFrameViewAsh::BrowserNonClientFrameViewAsh(
    BrowserFrame* frame, BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      caption_button_container_(NULL),
      window_icon_(NULL),
      frame_painter_(new ash::FramePainter) {
}

BrowserNonClientFrameViewAsh::~BrowserNonClientFrameViewAsh() {
}

void BrowserNonClientFrameViewAsh::Init() {
  caption_button_container_ = new ash::FrameCaptionButtonContainerView(frame(),
      ash::FrameCaptionButtonContainerView::MINIMIZE_ALLOWED);
  AddChildView(caption_button_container_);

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }

  // Create incognito icon if necessary.
  UpdateAvatarInfo();

  // Frame painter handles layout.
  frame_painter_->Init(frame(), window_icon_, caption_button_container_);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView overrides:

gfx::Rect BrowserNonClientFrameViewAsh::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  if (!tabstrip)
    return gfx::Rect();

  // When the tab strip is painted in the immersive fullscreen light bar style,
  // the caption buttons and the avatar button are not visible. However, their
  // bounds are still used to compute the tab strip bounds so that the tabs have
  // the same horizontal position when the tab strip is painted in the immersive
  // light bar style as when the top-of-window views are revealed.
  TabStripInsets insets(GetTabStripInsets(false));
  return gfx::Rect(insets.left, insets.top,
                   std::max(0, width() - insets.left - insets.right),
                   tabstrip->GetPreferredSize().height());
}

BrowserNonClientFrameView::TabStripInsets
BrowserNonClientFrameViewAsh::GetTabStripInsets(bool force_restored) const {
  int left = avatar_button() ? kAvatarSideSpacing +
      browser_view()->GetOTRAvatarIcon().width() + kAvatarSideSpacing :
      kTabstripLeftSpacing;
  int extra_right = ash::switches::UseAlternateFrameCaptionButtonStyle() ?
      kTabstripRightSpacingAlternateCaptionButtonStyle :
      kTabstripRightSpacing;
  int right = frame_painter_->GetRightInset() + extra_right;

  int top = NonClientTopBorderHeight();
  if (force_restored)
    top = kTabstripTopSpacingTall;

  if (browser_view()->IsTabStripVisible() &&
      !UseImmersiveLightbarHeaderStyle() &&
      !force_restored) {
    if (UseShortHeader()) {
      if (ash::switches::UseAlternateFrameCaptionButtonStyle())
        top += kTabstripTopSpacingShortAlternateCaptionButtonStyle;
      else
        top += kTabstripTopSpacingShort;
    } else {
      top += kTabstripTopSpacingTall;
    }
  }

  return TabStripInsets(top, left, right);
}

int BrowserNonClientFrameViewAsh::GetThemeBackgroundXInset() const {
  return frame_painter_->GetThemeBackgroundXInset();
}

void BrowserNonClientFrameViewAsh::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

///////////////////////////////////////////////////////////////////////////////
// views::NonClientFrameView overrides:

gfx::Rect BrowserNonClientFrameViewAsh::GetBoundsForClientView() const {
  int top_height = NonClientTopBorderHeight();
  return frame_painter_->GetBoundsForClientView(top_height, bounds());
}

gfx::Rect BrowserNonClientFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  return frame_painter_->GetWindowBoundsForClientBounds(top_height,
                                                        client_bounds);
}

int BrowserNonClientFrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  int hit_test = frame_painter_->NonClientHitTest(this, point);

  // See if the point is actually within the avatar menu button or within
  // the avatar label.
  if (hit_test == HTCAPTION && ((avatar_button() &&
       avatar_button()->GetMirroredBounds().Contains(point)) ||
      (avatar_label() && avatar_label()->GetMirroredBounds().Contains(point))))
      return HTCLIENT;

  // When the window is restored we want a large click target above the tabs
  // to drag the window, so redirect clicks in the tab's shadow to caption.
  if (hit_test == HTCLIENT &&
      !(frame()->IsMaximized() || frame()->IsFullscreen())) {
    // Convert point to client coordinates.
    gfx::Point client_point(point);
    View::ConvertPointToTarget(this, frame()->client_view(), &client_point);
    // Report hits in shadow at top of tabstrip as caption.
    gfx::Rect tabstrip_bounds(browser_view()->tabstrip()->bounds());
    if (client_point.y() < tabstrip_bounds.y() + kTabShadowHeight)
      hit_test = HTCAPTION;
  }
  return hit_test;
}

void BrowserNonClientFrameViewAsh::GetWindowMask(const gfx::Size& size,
                                                  gfx::Path* window_mask) {
  // Aura does not use window masks.
}

void BrowserNonClientFrameViewAsh::ResetWindowControls() {
  // Hide the caption buttons in immersive fullscreen when the tab light bar
  // is visible because it's confusing when the user hovers or clicks in the
  // top-right of the screen and hits one.
  bool button_visibility = !UseImmersiveLightbarHeaderStyle();
  caption_button_container_->SetVisible(button_visibility);

  caption_button_container_->ResetWindowControls();
}

void BrowserNonClientFrameViewAsh::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void BrowserNonClientFrameViewAsh::UpdateWindowTitle() {
  if (!frame()->IsFullscreen())
    frame_painter_->SchedulePaintForTitle(BrowserFrame::GetTitleFont());
}

///////////////////////////////////////////////////////////////////////////////
// views::View overrides:

void BrowserNonClientFrameViewAsh::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldPaint())
    return;

  if (UseImmersiveLightbarHeaderStyle()) {
    PaintImmersiveLightbarStyleHeader(canvas);
    return;
  }

  // The primary header image changes based on window activation state and
  // theme, so we look it up for each paint.
  int theme_frame_image_id = GetThemeFrameImageId();
  int theme_frame_overlay_image_id = GetThemeFrameOverlayImageId();

  ui::ThemeProvider* theme_provider = GetThemeProvider();
  ash::FramePainter::Themed header_themed = ash::FramePainter::THEMED_NO;
  if (theme_provider->HasCustomImage(theme_frame_image_id) ||
      (theme_frame_overlay_image_id != 0 &&
       theme_provider->HasCustomImage(theme_frame_overlay_image_id))) {
    header_themed = ash::FramePainter::THEMED_YES;
  }

  if (frame_painter_->ShouldUseMinimalHeaderStyle(header_themed))
    theme_frame_image_id = IDR_AURA_WINDOW_HEADER_BASE_MINIMAL;

  frame_painter_->PaintHeader(
      this,
      canvas,
      ShouldPaintAsActive() ?
          ash::FramePainter::ACTIVE : ash::FramePainter::INACTIVE,
      theme_frame_image_id,
      theme_frame_overlay_image_id);
  if (browser_view()->ShouldShowWindowTitle())
    frame_painter_->PaintTitleBar(this, canvas, BrowserFrame::GetTitleFont());
  if (browser_view()->IsToolbarVisible())
    PaintToolbarBackground(canvas);
  else
    PaintContentEdge(canvas);
}

void BrowserNonClientFrameViewAsh::Layout() {
  frame_painter_->LayoutHeader(this, UseShortHeader());
  if (avatar_button())
    LayoutAvatar();
  BrowserNonClientFrameView::Layout();
}

const char* BrowserNonClientFrameViewAsh::GetClassName() const {
  return kViewClassName;
}

bool BrowserNonClientFrameViewAsh::HitTestRect(const gfx::Rect& rect) const {
  if (!views::View::HitTestRect(rect)) {
    // |rect| is outside BrowserNonClientFrameViewAsh's bounds.
    return false;
  }
  // If the rect is outside the bounds of the client area, claim it.
  // TODO(tdanderson): Implement View::ConvertRectToTarget().
  gfx::Point rect_in_client_view_coords_origin(rect.origin());
  View::ConvertPointToTarget(this, frame()->client_view(),
      &rect_in_client_view_coords_origin);
  gfx::Rect rect_in_client_view_coords(
      rect_in_client_view_coords_origin, rect.size());
  if (!frame()->client_view()->HitTestRect(rect_in_client_view_coords))
    return true;

  // Otherwise, claim |rect| only if it is above the bottom of the tabstrip in
  // a non-tab portion.
  TabStrip* tabstrip = browser_view()->tabstrip();
  if (!tabstrip || !browser_view()->IsTabStripVisible())
    return false;

  gfx::Point rect_in_tabstrip_coords_origin(rect.origin());
  View::ConvertPointToTarget(this, tabstrip,
      &rect_in_tabstrip_coords_origin);
  gfx::Rect rect_in_tabstrip_coords(rect_in_tabstrip_coords_origin,
      rect.size());

  if (rect_in_tabstrip_coords.bottom() > tabstrip->GetLocalBounds().bottom()) {
    // |rect| is below the tabstrip.
    return false;
  }

  if (tabstrip->HitTestRect(rect_in_tabstrip_coords)) {
    // Claim |rect| if it is in a non-tab portion of the tabstrip.
    // TODO(tdanderson): Pass |rect_in_tabstrip_coords| instead of its center
    // point to TabStrip::IsPositionInWindowCaption() once
    // GetEventHandlerForRect() is implemented.
    return tabstrip->IsPositionInWindowCaption(
        rect_in_tabstrip_coords.CenterPoint());
  }

  // We claim |rect| because it is above the bottom of the tabstrip, but
  // not in the tabstrip. In particular, the window controls are right of
  // the tabstrip.
  return true;
}

void BrowserNonClientFrameViewAsh::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TITLEBAR;
}

gfx::Size BrowserNonClientFrameViewAsh::GetMinimumSize() {
  return frame_painter_->GetMinimumSize(this);
}

void BrowserNonClientFrameViewAsh::OnThemeChanged() {
  BrowserNonClientFrameView::OnThemeChanged();
  frame_painter_->OnThemeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// chrome::TabIconViewModel overrides:

bool BrowserNonClientFrameViewAsh::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // WebContents because in this condition there is not yet a selected tab.
  content::WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab ? current_tab->IsLoading() : false;
}

gfx::ImageSkia BrowserNonClientFrameViewAsh::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (!delegate)
    return gfx::ImageSkia();
  return delegate->GetWindowIcon();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, private:

int BrowserNonClientFrameViewAsh::NonClientTopBorderHeight() const {
  if (!ShouldPaint() || browser_view()->IsTabStripVisible())
    return 0;

  int caption_buttons_bottom = caption_button_container_->bounds().bottom();
  if (browser_view()->IsToolbarVisible())
    return caption_buttons_bottom - kContentShadowHeight;
  return caption_buttons_bottom + kClientEdgeThickness;
}

bool BrowserNonClientFrameViewAsh::UseShortHeader() const {
  // Restored browser -> tall header
  // Maximized browser -> short header
  // Fullscreen browser, no immersive reveal -> hidden or super short light bar
  // Fullscreen browser, immersive reveal -> short header
  // Popup&App window -> tall header
  // Panels use short header and are handled via ash::PanelFrameView.
  // Dialogs use short header and are handled via ash::CustomFrameViewAsh.
  Browser* browser = browser_view()->browser();
  switch (browser->type()) {
    case Browser::TYPE_TABBED:
      return frame()->IsMaximized() || frame()->IsFullscreen();
    case Browser::TYPE_POPUP:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool BrowserNonClientFrameViewAsh::UseImmersiveLightbarHeaderStyle() const {
  ImmersiveModeController* immersive_controller =
      browser_view()->immersive_mode_controller();
  return immersive_controller->IsEnabled() &&
      !immersive_controller->IsRevealed() &&
      browser_view()->IsTabStripVisible();
}

void BrowserNonClientFrameViewAsh::LayoutAvatar() {
  DCHECK(avatar_button());
  gfx::ImageSkia incognito_icon = browser_view()->GetOTRAvatarIcon();

  int avatar_bottom = GetTabStripInsets(false).top +
      browser_view()->GetTabStripHeight() - kAvatarBottomSpacing;
  int avatar_restored_y = avatar_bottom - incognito_icon.height();
  int avatar_y = (frame()->IsMaximized() || frame()->IsFullscreen()) ?
      GetTabStripInsets(false).top + kContentShadowHeight :
      avatar_restored_y;

  // Hide the incognito icon in immersive fullscreen when the tab light bar is
  // visible because the header is too short for the icognito icon to be
  // recognizable.
  bool avatar_visible = !UseImmersiveLightbarHeaderStyle();
  int avatar_height = avatar_visible ? avatar_bottom - avatar_y : 0;

  gfx::Rect avatar_bounds(kAvatarSideSpacing,
                          avatar_y,
                          incognito_icon.width(),
                          avatar_height);
  avatar_button()->SetBoundsRect(avatar_bounds);
  avatar_button()->SetVisible(avatar_visible);
}

bool BrowserNonClientFrameViewAsh::ShouldPaint() const {
  if (!frame()->IsFullscreen())
    return true;

  // We need to paint when in immersive fullscreen and either:
  // - The top-of-window views are revealed.
  // - The lightbar style tabstrip is visible.
  // Because immersive fullscreen is only supported for tabbed browser windows,
  // checking whether the tab strip is visible is sufficient.
  return browser_view()->IsTabStripVisible();
}

void BrowserNonClientFrameViewAsh::PaintImmersiveLightbarStyleHeader(
    gfx::Canvas* canvas) {
  // The light bar header is not themed because theming it does not look good.
  gfx::ImageSkia* frame_image = GetThemeProvider()->GetImageSkiaNamed(
      IDR_AURA_WINDOW_HEADER_BASE_MINIMAL);
  canvas->TileImageInt(*frame_image, 0, 0, width(), frame_image->height());
}

void BrowserNonClientFrameViewAsh::PaintToolbarBackground(gfx::Canvas* canvas) {
  gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  if (toolbar_bounds.IsEmpty())
    return;
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToTarget(browser_view(), this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);

  int x = toolbar_bounds.x();
  int w = toolbar_bounds.width();
  int y = toolbar_bounds.y();
  int h = toolbar_bounds.height();

  // Gross hack: We split the toolbar images into two pieces, since sometimes
  // (popup mode) the toolbar isn't tall enough to show the whole image.  The
  // split happens between the top shadow section and the bottom gradient
  // section so that we never break the gradient.
  int split_point = kFrameShadowThickness * 2;
  int bottom_y = y + split_point;
  ui::ThemeProvider* tp = GetThemeProvider();
  int bottom_edge_height = h - split_point;

  canvas->FillRect(gfx::Rect(x, bottom_y, w, bottom_edge_height),
                   tp->GetColor(ThemeProperties::COLOR_TOOLBAR));

  // Paint the main toolbar image.  Since this image is also used to draw the
  // tab background, we must use the tab strip offset to compute the image
  // source y position.  If you have to debug this code use an image editor
  // to paint a diagonal line through the toolbar image and ensure it lines up
  // across the tab and toolbar.
  gfx::ImageSkia* theme_toolbar = tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR);
  canvas->TileImageInt(
      *theme_toolbar,
      x + GetThemeBackgroundXInset(),
      bottom_y - GetTabStripInsets(false).top,
      x, bottom_y,
      w, theme_toolbar->height());

  // The content area line has a shadow that extends a couple of pixels above
  // the toolbar bounds.
  const int kContentShadowHeight = 2;
  gfx::ImageSkia* toolbar_top = tp->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_TOP);
  canvas->TileImageInt(*toolbar_top,
                       0, 0,
                       x, y - kContentShadowHeight,
                       w, split_point + kContentShadowHeight + 1);

  // Draw the "lightening" shade line around the edges of the toolbar.
  gfx::ImageSkia* toolbar_left = tp->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_LEFT);
  canvas->TileImageInt(*toolbar_left,
                       0, 0,
                       x + kClientEdgeThickness,
                       y + kClientEdgeThickness + kContentShadowHeight,
                       toolbar_left->width(), theme_toolbar->height());
  gfx::ImageSkia* toolbar_right =
      tp->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_RIGHT);
  canvas->TileImageInt(*toolbar_right,
                       0, 0,
                       w - toolbar_right->width() - 2 * kClientEdgeThickness,
                       y + kClientEdgeThickness + kContentShadowHeight,
                       toolbar_right->width(), theme_toolbar->height());

  // Draw the content/toolbar separator.
  canvas->FillRect(
      gfx::Rect(x + kClientEdgeThickness,
                toolbar_bounds.bottom() - kClientEdgeThickness,
                w - (2 * kClientEdgeThickness),
                kClientEdgeThickness),
      ThemeProperties::GetDefaultColor(
          ThemeProperties::COLOR_TOOLBAR_SEPARATOR));
}

void BrowserNonClientFrameViewAsh::PaintContentEdge(gfx::Canvas* canvas) {
  canvas->FillRect(gfx::Rect(0, caption_button_container_->bounds().bottom(),
                             width(), kClientEdgeThickness),
      ThemeProperties::GetDefaultColor(
          ThemeProperties::COLOR_TOOLBAR_SEPARATOR));
}

int BrowserNonClientFrameViewAsh::GetThemeFrameImageId() const {
  bool is_incognito = browser_view()->IsOffTheRecord() &&
                      !browser_view()->IsGuestSession();
  if (browser_view()->IsBrowserTypeNormal()) {
    // Use the standard resource ids to allow users to theme the frames.
    if (ShouldPaintAsActive()) {
      return is_incognito ?
          IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME;
    }
    return is_incognito ?
        IDR_THEME_FRAME_INCOGNITO_INACTIVE : IDR_THEME_FRAME_INACTIVE;
  }
  // Never theme app and popup windows.
  if (ShouldPaintAsActive()) {
    return is_incognito ?
        IDR_AURA_WINDOW_HEADER_BASE_INCOGNITO_ACTIVE :
        IDR_AURA_WINDOW_HEADER_BASE_ACTIVE;
  }
  return is_incognito ?
      IDR_AURA_WINDOW_HEADER_BASE_INCOGNITO_INACTIVE :
      IDR_AURA_WINDOW_HEADER_BASE_INACTIVE;
}

int BrowserNonClientFrameViewAsh::GetThemeFrameOverlayImageId() const {
  ui::ThemeProvider* tp = GetThemeProvider();
  if (tp->HasCustomImage(IDR_THEME_FRAME_OVERLAY) &&
      browser_view()->IsBrowserTypeNormal() &&
      !browser_view()->IsOffTheRecord()) {
    return ShouldPaintAsActive() ?
        IDR_THEME_FRAME_OVERLAY : IDR_THEME_FRAME_OVERLAY_INACTIVE;
  }
  return 0;
}
