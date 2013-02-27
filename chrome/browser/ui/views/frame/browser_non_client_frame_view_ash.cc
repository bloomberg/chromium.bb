// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include "ash/ash_switches.h"
#include "ash/wm/frame_painter.h"
#include "ash/wm/workspace/frame_maximize_button.h"
#include "base/command_line.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/avatar_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "content/public/browser/web_contents.h"
#include "grit/ash_resources.h"
#include "grit/generated_resources.h"  // Accessibility names
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
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using ash::switches::kAshImmersiveMode;

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

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, public:

// static
const char BrowserNonClientFrameViewAsh::kViewClassName[] =
    "BrowserNonClientFrameViewAsh";

BrowserNonClientFrameViewAsh::BrowserNonClientFrameViewAsh(
    BrowserFrame* frame, BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      size_button_(NULL),
      close_button_(NULL),
      window_icon_(NULL),
      frame_painter_(new ash::FramePainter),
      size_button_minimizes_(false) {
}

BrowserNonClientFrameViewAsh::~BrowserNonClientFrameViewAsh() {
}

void BrowserNonClientFrameViewAsh::Init() {
  // Panels only minimize.
  ash::FramePainter::SizeButtonBehavior size_button_behavior;
  if (browser_view()->browser()->is_type_panel() &&
      browser_view()->browser()->app_type() == Browser::APP_TYPE_CHILD) {
    size_button_minimizes_ = true;
    size_button_ = new views::ImageButton(this);
    size_button_behavior = ash::FramePainter::SIZE_BUTTON_MINIMIZES;
  } else {
    size_button_ = new ash::FrameMaximizeButton(this, this);
    size_button_behavior = ash::FramePainter::SIZE_BUTTON_MAXIMIZES;
  }
  size_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_MAXIMIZE));
  AddChildView(size_button_);
  close_button_ = new views::ImageButton(this);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }

  // Create incognito icon if necessary.
  UpdateAvatarInfo();

  // Frame painter handles layout of these buttons.
  frame_painter_->Init(frame(), window_icon_, size_button_, close_button_,
                       size_button_behavior);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView overrides:

gfx::Rect BrowserNonClientFrameViewAsh::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  if (!tabstrip)
    return gfx::Rect();
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
  int right = frame_painter_->GetRightInset() + kTabstripRightSpacing;
  return TabStripInsets(NonClientTopBorderHeight(force_restored), left, right);
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
  int top_height = NonClientTopBorderHeight(false);
  return frame_painter_->GetBoundsForClientView(top_height, bounds());
}

gfx::Rect BrowserNonClientFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight(false);
  return frame_painter_->GetWindowBoundsForClientBounds(top_height,
                                                        client_bounds);
}

int BrowserNonClientFrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  int hit_test = frame_painter_->NonClientHitTest(this, point);
  // When the window is restored we want a large click target above the tabs
  // to drag the window, so redirect clicks in the tab's shadow to caption.
  if (hit_test == HTCLIENT && !frame()->IsMaximized()) {
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
  if (CommandLine::ForCurrentProcess()->HasSwitch(kAshImmersiveMode)) {
    // Hide the caption buttons in immersive mode because it's confusing when
    // the user hovers or clicks in the top-right of the screen and hits one.
    // Only show them during a reveal.
    ImmersiveModeController* controller =
        browser_view()->immersive_mode_controller();
    if (controller->enabled()) {
      bool revealed = controller->IsRevealed();
      size_button_->SetVisible(revealed);
      close_button_->SetVisible(revealed);
    } else {
      size_button_->SetVisible(true);
      close_button_->SetVisible(true);
    }
  }

  size_button_->SetState(views::CustomButton::STATE_NORMAL);
  // The close button isn't affected by this constraint.
}

void BrowserNonClientFrameViewAsh::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void BrowserNonClientFrameViewAsh::UpdateWindowTitle() {
  if (!frame()->IsFullscreen())
    frame_painter_->SchedulePaintForTitle(this, BrowserFrame::GetTitleFont());
}

///////////////////////////////////////////////////////////////////////////////
// views::View overrides:

void BrowserNonClientFrameViewAsh::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldPaint())
    return;
  // The primary header image changes based on window activation state and
  // theme, so we look it up for each paint.
  frame_painter_->PaintHeader(
      this,
      canvas,
      ShouldPaintAsActive() ?
          ash::FramePainter::ACTIVE : ash::FramePainter::INACTIVE,
      GetThemeFrameImageId(),
      GetThemeFrameOverlayImage());
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

std::string BrowserNonClientFrameViewAsh::GetClassName() const {
  return kViewClassName;
}

bool BrowserNonClientFrameViewAsh::HitTestRect(const gfx::Rect& rect) const {
  // If the rect is outside the bounds of the client area, claim it.
  if (NonClientFrameView::HitTestRect(rect))
    return true;

  // Otherwise claim it only if it's in a non-tab portion of the tabstrip.
  if (!browser_view()->tabstrip())
    return false;
  gfx::Rect tabstrip_bounds(browser_view()->tabstrip()->bounds());
  gfx::Point tabstrip_origin(tabstrip_bounds.origin());
  View::ConvertPointToTarget(frame()->client_view(), this, &tabstrip_origin);
  tabstrip_bounds.set_origin(tabstrip_origin);
  if (rect.bottom() > tabstrip_bounds.bottom())
    return false;

  // We convert from our parent's coordinates since we assume we fill its bounds
  // completely. We need to do this since we're not a parent of the tabstrip,
  // meaning ConvertPointToTarget would otherwise return something bogus.
  // TODO(tdanderson): Initialize |browser_view_point| using |rect| instead of
  // its center point once GetEventHandlerForRect() is implemented.
  gfx::Point browser_view_point(rect.CenterPoint());
  View::ConvertPointToTarget(parent(), browser_view(), &browser_view_point);
  return browser_view()->IsPositionInWindowCaption(browser_view_point);
}

void BrowserNonClientFrameViewAsh::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TITLEBAR;
}

gfx::Size BrowserNonClientFrameViewAsh::GetMinimumSize() {
  return frame_painter_->GetMinimumSize(this);
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:

void BrowserNonClientFrameViewAsh::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  // When shift-clicking slow down animations for visual debugging.
  // We used to do this via an event filter that looked for the shift key being
  // pressed but this interfered with several normal keyboard shortcuts.
  if (event.IsShiftDown())
    ui::LayerAnimator::set_slow_animation_mode(true);

  if (sender == size_button_) {
    // The maximize button may move out from under the cursor.
    ResetWindowControls();
    if (size_button_minimizes_)
      frame()->Minimize();
    else if (frame()->IsFullscreen())  // Can be clicked in immersive mode.
      frame()->SetFullscreen(false);
    else if (frame()->IsMaximized())
      frame()->Restore();
    else
      frame()->Maximize();
    // |this| may be deleted - some windows delete their frames on maximize.
  } else if (sender == close_button_) {
    frame()->Close();
  }

  if (event.IsShiftDown())
    ui::LayerAnimator::set_slow_animation_mode(false);
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


int BrowserNonClientFrameViewAsh::NonClientTopBorderHeight(
    bool force_restored) const {
  if (force_restored)
    return kTabstripTopSpacingTall;
  if (frame()->IsFullscreen())
    return 0;
  // Windows with tab strips need a smaller non-client area.
  if (browser_view()->IsTabStripVisible()) {
    if (UseShortHeader())
      return kTabstripTopSpacingShort;
    return kTabstripTopSpacingTall;
  }
  // For windows without a tab strip (popups, etc.) ensure we have enough space
  // to see the window caption buttons.
  return close_button_->bounds().bottom() - kContentShadowHeight;
}

bool BrowserNonClientFrameViewAsh::UseShortHeader() const {
  // Restored browser -> tall header
  // Maximized browser -> short header
  // Fullscreen browser (header shows with immersive reveal) -> short header
  // Popup&App window -> tall header
  // Panel -> short header
  // Dialogs use short header and are handled via CustomFrameViewAsh.
  Browser* browser = browser_view()->browser();
  switch (browser->type()) {
    case Browser::TYPE_TABBED:
      return frame()->IsMaximized() || frame()->IsFullscreen();
    case Browser::TYPE_POPUP:
      return false;
    case Browser::TYPE_PANEL:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

void BrowserNonClientFrameViewAsh::LayoutAvatar() {
  DCHECK(avatar_button());
  gfx::ImageSkia incognito_icon = browser_view()->GetOTRAvatarIcon();

  int avatar_bottom = GetTabStripInsets(false).top +
      browser_view()->GetTabStripHeight() - kAvatarBottomSpacing;
  int avatar_restored_y = avatar_bottom - incognito_icon.height();
  int avatar_y = frame()->IsMaximized() ?
      NonClientTopBorderHeight(false) + kContentShadowHeight:
      avatar_restored_y;
  gfx::Rect avatar_bounds(kAvatarSideSpacing,
                          avatar_y,
                          incognito_icon.width(),
                          avatar_bottom - avatar_y);
  avatar_button()->SetBoundsRect(avatar_bounds);
}

bool BrowserNonClientFrameViewAsh::ShouldPaint() const {
  // Immersive mode windows are fullscreen, but need to paint during a reveal.
  return !frame()->IsFullscreen() ||
      browser_view()->immersive_mode_controller()->IsRevealed();
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
  canvas->FillRect(gfx::Rect(0, close_button_->bounds().bottom(),
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

const gfx::ImageSkia*
BrowserNonClientFrameViewAsh::GetThemeFrameOverlayImage() const {
  ui::ThemeProvider* tp = GetThemeProvider();
  if (tp->HasCustomImage(IDR_THEME_FRAME_OVERLAY) &&
      browser_view()->IsBrowserTypeNormal() &&
      !browser_view()->IsOffTheRecord()) {
    return tp->GetImageSkiaNamed(ShouldPaintAsActive() ?
        IDR_THEME_FRAME_OVERLAY : IDR_THEME_FRAME_OVERLAY_INACTIVE);
  }
  return NULL;
}
