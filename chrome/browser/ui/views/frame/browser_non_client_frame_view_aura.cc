// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_aura.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"  // Accessibility names
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/aura/window.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// Size of border along top edge, used for resize handle computations.
const int kTopThickness = 1;
// TODO(jamescook): Border is specified to be a single pixel overlapping
// the web content and may need to be built into the shadow layers instead.
const int kBorderThickness = 0;
// Number of pixels outside the window frame to look for resize events.
const int kResizeAreaOutsideBounds = 6;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// Space between left edge of window and popup window icon.
const int kIconOffsetX = 4;
// Space between top of window and popup window icon.
const int kIconOffsetY = 4;
// Height and width of window icon.
const int kIconSize = 16;
// Space between the title text and the caption buttons.
const int kTitleLogoSpacing = 5;
// Space between title text and icon.
const int kTitleOffsetX = 4;
// Space between title text and top of window.
const int kTitleOffsetY = 6;
// Space between close button and right edge of window.
const int kCloseButtonOffsetX = 0;
// Space between close button and top edge of window.
const int kCloseButtonOffsetY = 0;
// Space between left edge of window and tabstrip.
const int kTabstripLeftSpacing = 4;
// Space between right edge of tabstrip and maximize button.
const int kTabstripRightSpacing = 10;
// Space between top of window and top of tabstrip for restored windows.
const int kTabstripTopSpacingRestored = 10;
// Space between top of window and top of tabstrip for maximized windows.
const int kTabstripTopSpacingMaximized = 1;

// Tiles an image into an area, rounding the top corners.
void TileRoundRect(gfx::Canvas* canvas,
                   int x, int y, int w, int h,
                   const SkBitmap& bitmap,
                   int corner_radius) {
  SkRect rect;
  rect.iset(x, y, x + w, y + h);
  const SkScalar kRadius = SkIntToScalar(corner_radius);
  SkScalar radii[8] = {
      kRadius, kRadius,  // top-left
      kRadius, kRadius,  // top-right
      0, 0,   // bottom-right
      0, 0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);

  SkPaint paint;
  SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                  SkShader::kRepeat_TileMode,
                                                  SkShader::kRepeat_TileMode);
  paint.setShader(shader);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  // CreateBitmapShader returns a Shader with a reference count of one, we
  // need to unref after paint takes ownership of the shader.
  shader->unref();
  canvas->GetSkCanvas()->drawPath(path, paint);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAura, public:

BrowserNonClientFrameViewAura::BrowserNonClientFrameViewAura(
    BrowserFrame* frame, BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      maximize_button_(NULL),
      close_button_(NULL),
      window_icon_(NULL),
      button_separator_(NULL),
      top_left_corner_(NULL),
      top_edge_(NULL),
      top_right_corner_(NULL),
      header_left_edge_(NULL),
      header_right_edge_(NULL) {
}

BrowserNonClientFrameViewAura::~BrowserNonClientFrameViewAura() {
}

void BrowserNonClientFrameViewAura::Init() {
  // Ensure we get resize cursors for a few pixels outside our bounds.
  frame()->GetNativeWindow()->set_hit_test_bounds_inset(
      -kResizeAreaOutsideBounds);

  // Caption buttons.
  maximize_button_ = new views::ImageButton(this);
  SetButtonImages(maximize_button_,
                  IDR_AURA_WINDOW_MAXIMIZE,
                  IDR_AURA_WINDOW_MAXIMIZE_H,
                  IDR_AURA_WINDOW_MAXIMIZE_P);
  maximize_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_MAXIMIZE));
  AddChildView(maximize_button_);
  close_button_ = new views::ImageButton(this);
  SetButtonImages(close_button_,
                  IDR_AURA_WINDOW_CLOSE,
                  IDR_AURA_WINDOW_CLOSE_H,
                  IDR_AURA_WINDOW_CLOSE_P);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  // Window frame image parts.
  ResourceBundle& bundle = ResourceBundle::GetSharedInstance();
  button_separator_ =
      bundle.GetBitmapNamed(IDR_AURA_WINDOW_BUTTON_SEPARATOR);
  top_left_corner_ =
      bundle.GetBitmapNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP_LEFT);
  top_edge_ =
      bundle.GetBitmapNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP);
  top_right_corner_ =
      bundle.GetBitmapNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP_RIGHT);
  header_left_edge_ =
      bundle.GetBitmapNamed(IDR_AURA_WINDOW_HEADER_SHADE_LEFT);
  header_right_edge_ =
      bundle.GetBitmapNamed(IDR_AURA_WINDOW_HEADER_SHADE_RIGHT);
  // TODO(jamescook): Should we paint the header bottom edge here, or keep
  // it in BrowserFrameAura::ToolbarBackground as we do now?

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView overrides:

gfx::Rect BrowserNonClientFrameViewAura::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  if (!tabstrip)
    return gfx::Rect();
  bool restored = !frame()->IsMaximized();
  return gfx::Rect(kTabstripLeftSpacing,
                   GetHorizontalTabStripVerticalOffset(restored),
                   maximize_button_->x() - kTabstripRightSpacing,
                   tabstrip->GetPreferredSize().height());
}

int BrowserNonClientFrameViewAura::GetHorizontalTabStripVerticalOffset(
    bool restored) const {
  return NonClientTopBorderHeight(restored);
}

void BrowserNonClientFrameViewAura::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

///////////////////////////////////////////////////////////////////////////////
// views::NonClientFrameView overrides:

gfx::Rect BrowserNonClientFrameViewAura::GetBoundsForClientView() const {
  int top_height = NonClientTopBorderHeight(false);
  return gfx::Rect(kBorderThickness,
                   top_height,
                   std::max(0, width() - (2 * kBorderThickness)),
                   std::max(0, height() - top_height - kBorderThickness));
}

gfx::Rect BrowserNonClientFrameViewAura::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight(false);
  return gfx::Rect(std::max(0, client_bounds.x() - kBorderThickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * kBorderThickness),
                   client_bounds.height() + top_height + kBorderThickness);
}

int BrowserNonClientFrameViewAura::NonClientHitTest(const gfx::Point& point) {
  gfx::Rect expanded_bounds = bounds();
  expanded_bounds.Inset(-kResizeAreaOutsideBounds, -kResizeAreaOutsideBounds);
  if (!expanded_bounds.Contains(point))
    return HTNOWHERE;

  // No avatar button for Chrome OS.

  // Check the client view first, as it overlaps the window caption area.
  int client_component = frame()->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->visible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;
  if (maximize_button_->visible() &&
      maximize_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;

  bool can_resize = frame()->widget_delegate() ?
      frame()->widget_delegate()->CanResize() :
      false;
  int frame_component = GetHTComponentForFrame(point,
                                               kTopThickness,
                                               kBorderThickness,
                                               kResizeAreaCornerSize,
                                               kResizeAreaCornerSize,
                                               can_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Caption is a safe default.
  return HTCAPTION;
}

void BrowserNonClientFrameViewAura::GetWindowMask(const gfx::Size& size,
                                                  gfx::Path* window_mask) {
  // Aura does not use window masks.
}

void BrowserNonClientFrameViewAura::ResetWindowControls() {
  maximize_button_->SetState(views::CustomButton::BS_NORMAL);
  // The close button isn't affected by this constraint.
}

void BrowserNonClientFrameViewAura::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// views::View overrides:

void BrowserNonClientFrameViewAura::OnPaint(gfx::Canvas* canvas) {
  if (frame()->IsFullscreen())
    return;  // Nothing visible, don't paint.
  PaintHeader(canvas);
  PaintTitleBar(canvas);
  PaintToolbarBackground(canvas);
  // Paint the view hierarchy, which draws the caption buttons.
  BrowserNonClientFrameView::OnPaint(canvas);
}

void BrowserNonClientFrameViewAura::Layout() {
  // Maximized windows and app/popup windows use shorter buttons.
  if (frame()->IsMaximized() ||
      !browser_view()->IsBrowserTypeNormal()) {
    SetButtonImages(close_button_,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE_H,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE_P);
    SetButtonImages(maximize_button_,
                    IDR_AURA_WINDOW_MAXIMIZED_RESTORE,
                    IDR_AURA_WINDOW_MAXIMIZED_RESTORE_H,
                    IDR_AURA_WINDOW_MAXIMIZED_RESTORE_P);
  } else {
    SetButtonImages(close_button_,
                    IDR_AURA_WINDOW_CLOSE,
                    IDR_AURA_WINDOW_CLOSE_H,
                    IDR_AURA_WINDOW_CLOSE_P);
    SetButtonImages(maximize_button_,
                    IDR_AURA_WINDOW_MAXIMIZE,
                    IDR_AURA_WINDOW_MAXIMIZE_H,
                    IDR_AURA_WINDOW_MAXIMIZE_P);
  }

  gfx::Size close_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      width() - close_size.width() - kCloseButtonOffsetX,
      kCloseButtonOffsetY,
      close_size.width(),
      close_size.height());

  gfx::Size maximize_size = maximize_button_->GetPreferredSize();
  maximize_button_->SetBounds(
      close_button_->x() - button_separator_->width() - maximize_size.width(),
      close_button_->y(),
      maximize_size.width(),
      maximize_size.height());

  if (window_icon_)
    window_icon_->SetBoundsRect(
        gfx::Rect(kIconOffsetX, kIconOffsetY, kIconSize, kIconSize));

  BrowserNonClientFrameView::Layout();
}

bool BrowserNonClientFrameViewAura::HitTest(const gfx::Point& l) const {
  // If the point is outside the bounds of the client area, claim it.
  if (NonClientFrameView::HitTest(l))
    return true;

  // Otherwise claim it only if it's in a non-tab portion of the tabstrip.
  if (!browser_view()->tabstrip())
    return false;
  gfx::Rect tabstrip_bounds(browser_view()->tabstrip()->bounds());
  gfx::Point tabstrip_origin(tabstrip_bounds.origin());
  View::ConvertPointToView(frame()->client_view(), this, &tabstrip_origin);
  tabstrip_bounds.set_origin(tabstrip_origin);
  if (l.y() > tabstrip_bounds.bottom())
    return false;

  // We convert from our parent's coordinates since we assume we fill its bounds
  // completely. We need to do this since we're not a parent of the tabstrip,
  // meaning ConvertPointToView would otherwise return something bogus.
  gfx::Point browser_view_point(l);
  View::ConvertPointToView(parent(), browser_view(), &browser_view_point);
  return browser_view()->IsPositionInWindowCaption(browser_view_point);
}

void BrowserNonClientFrameViewAura::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TITLEBAR;
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:

void BrowserNonClientFrameViewAura::ButtonPressed(views::Button* sender,
                                                  const views::Event& event) {
  if (sender == maximize_button_) {
    if (frame()->IsMaximized())
      frame()->Restore();
    else
      frame()->Maximize();
    // The maximize button may have moved out from under the cursor.
    ResetWindowControls();
  } else if (sender == close_button_) {
    frame()->Close();
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabIconView::TabIconViewModel overrides:

bool BrowserNonClientFrameViewAura::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // WebContents because in this condition there is not yet a selected tab.
  content::WebContents* current_tab = browser_view()->GetSelectedWebContents();
  return current_tab ? current_tab->IsLoading() : false;
}

SkBitmap BrowserNonClientFrameViewAura::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (!delegate)
    return SkBitmap();
  return delegate->GetWindowIcon();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAura, private:

void BrowserNonClientFrameViewAura::SetButtonImages(views::ImageButton* button,
                                                    int normal_bitmap_id,
                                                    int hot_bitmap_id,
                                                    int pushed_bitmap_id) {
  ui::ThemeProvider* tp = frame()->GetThemeProvider();
  button->SetImage(views::CustomButton::BS_NORMAL,
                   tp->GetBitmapNamed(normal_bitmap_id));
  button->SetImage(views::CustomButton::BS_HOT,
                   tp->GetBitmapNamed(hot_bitmap_id));
  button->SetImage(views::CustomButton::BS_PUSHED,
                   tp->GetBitmapNamed(pushed_bitmap_id));
}

int BrowserNonClientFrameViewAura::NonClientTopBorderHeight(
    bool restored) const {
  if (frame()->widget_delegate() &&
      frame()->widget_delegate()->ShouldShowWindowTitle()) {
    // For popups ensure we have enough space to see the full window buttons.
    return kCloseButtonOffsetY + close_button_->height();
  }
  if (restored)
    return kTabstripTopSpacingRestored;
  return kTabstripTopSpacingMaximized;
}

void BrowserNonClientFrameViewAura::PaintHeader(gfx::Canvas* canvas) {
  // The primary header image changes based on window activation state and
  // theme, so we look it up for each paint.
  SkBitmap* theme_frame = GetThemeFrameBitmap();
  SkBitmap* theme_frame_overlay = GetThemeFrameOverlayBitmap();

  // Draw the header background, clipping the corners to be rounded.
  const int kCornerRadius = 2;
  TileRoundRect(canvas,
                0, 0, width(), theme_frame->height(),
                *theme_frame,
                kCornerRadius);

  // Draw the theme frame overlay, if available.
  if (theme_frame_overlay)
    canvas->DrawBitmapInt(*theme_frame_overlay, 0, 0);

  // Separator between the maximize and close buttons.
  canvas->DrawBitmapInt(*button_separator_,
                        close_button_->x() - button_separator_->width(),
                        close_button_->y());

  // Draw the top corners and edge.
  int top_left_height = top_left_corner_->height();
  canvas->DrawBitmapInt(*top_left_corner_,
                        0, 0, top_left_corner_->width(), top_left_height,
                        0, 0, top_left_corner_->width(), top_left_height,
                        false);
  canvas->TileImageInt(*top_edge_,
      top_left_corner_->width(),
      0,
      width() - top_left_corner_->width() - top_right_corner_->width(),
      top_edge_->height());
  int top_right_height = top_right_corner_->height();
  canvas->DrawBitmapInt(*top_right_corner_,
                        0, 0,
                        top_right_corner_->width(), top_right_height,
                        width() - top_right_corner_->width(), 0,
                        top_right_corner_->width(), top_right_height,
                        false);

  // Header left edge.
  int header_left_height = theme_frame->height() - top_left_height;
  canvas->TileImageInt(*header_left_edge_,
                       0, top_left_height,
                       header_left_edge_->width(), header_left_height);

  // Header right edge.
  int header_right_height = theme_frame->height() - top_right_height;
  canvas->TileImageInt(*header_right_edge_,
                       width() - header_right_edge_->width(), top_right_height,
                       header_right_edge_->width(), header_right_height);

  // We don't draw edges around the content area.  Web content goes flush
  // to the edge of the window.
}

void BrowserNonClientFrameViewAura::PaintToolbarBackground(
    gfx::Canvas* canvas) {
  gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  if (toolbar_bounds.IsEmpty())
    return;
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  ConvertPointToView(browser_view(), this, &toolbar_origin);
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
  SkBitmap* theme_toolbar = tp->GetBitmapNamed(IDR_THEME_TOOLBAR);
  int bottom_edge_height = std::min(theme_toolbar->height(), h) - split_point;

  canvas->FillRect(gfx::Rect(x, bottom_y, w, bottom_edge_height),
                   tp->GetColor(ThemeService::COLOR_TOOLBAR));

  // Paint the main toolbar image.  Since this image is also used to draw the
  // tab background, we must use the tab strip offset to compute the image
  // source y position.  If you have to debug this code use an image editor
  // to paint a diagonal line through the toolbar image and ensure it lines up
  // across the tab and toolbar.
  bool restored = !frame()->IsMaximized();
  canvas->TileImageInt(
      *theme_toolbar,
      x, bottom_y - GetHorizontalTabStripVerticalOffset(restored),
      x, bottom_y,
      w, theme_toolbar->height());

  SkBitmap* toolbar_center =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_CENTER);
  canvas->TileImageInt(*toolbar_center,
                       0, 0,
                       x, y,
                       w, split_point);

  // Draw the content/toolbar separator.
  canvas->FillRect(gfx::Rect(x + kClientEdgeThickness,
                             toolbar_bounds.bottom() - kClientEdgeThickness,
                             w - (2 * kClientEdgeThickness),
                             kClientEdgeThickness),
      ThemeService::GetDefaultColor(ThemeService::COLOR_TOOLBAR_SEPARATOR));
}

void BrowserNonClientFrameViewAura::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by the TabIconView.
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (delegate && delegate->ShouldShowWindowTitle()) {
    int icon_right = window_icon_ ? window_icon_->bounds().right() : 0;
    gfx::Rect title_bounds(
        icon_right + kTitleOffsetX,
        kTitleOffsetY,
        std::max(0, maximize_button_->x() - kTitleLogoSpacing - icon_right),
        BrowserFrame::GetTitleFont().GetHeight());
    canvas->DrawStringInt(delegate->GetWindowTitle(),
                          BrowserFrame::GetTitleFont(),
                          SK_ColorBLACK,
                          GetMirroredXForRect(title_bounds),
                          title_bounds.y(),
                          title_bounds.width(),
                          title_bounds.height());
  }
}

SkBitmap* BrowserNonClientFrameViewAura::GetThemeFrameBitmap() const {
  bool is_incognito = browser_view()->IsOffTheRecord();
  int resource_id;
  if (browser_view()->IsBrowserTypeNormal()) {
    if (ShouldPaintAsActive()) {
      // Use the standard resource ids to allow users to theme the frames.
      // TODO(jamescook): If this becomes the only frame we use on Aura, define
      // the resources to use the standard ids like IDR_THEME_FRAME, etc.
      if (is_incognito) {
        return GetCustomBitmap(IDR_THEME_FRAME_INCOGNITO,
                               IDR_AURA_WINDOW_HEADER_BASE_INCOGNITO_ACTIVE);
      }
      return GetCustomBitmap(IDR_THEME_FRAME,
                             IDR_AURA_WINDOW_HEADER_BASE_ACTIVE);
    }
    if (is_incognito) {
      return GetCustomBitmap(IDR_THEME_FRAME_INCOGNITO_INACTIVE,
                             IDR_AURA_WINDOW_HEADER_BASE_INCOGNITO_INACTIVE);
    }
    return GetCustomBitmap(IDR_THEME_FRAME_INACTIVE,
                           IDR_AURA_WINDOW_HEADER_BASE_INACTIVE);
  }
  // Never theme app and popup windows.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (ShouldPaintAsActive()) {
    resource_id = is_incognito ?
        IDR_AURA_WINDOW_HEADER_BASE_INCOGNITO_ACTIVE :
        IDR_AURA_WINDOW_HEADER_BASE_ACTIVE;
  } else {
    resource_id = is_incognito ?
        IDR_AURA_WINDOW_HEADER_BASE_INCOGNITO_INACTIVE :
        IDR_AURA_WINDOW_HEADER_BASE_INACTIVE;
  }
  return rb.GetBitmapNamed(resource_id);
}

SkBitmap* BrowserNonClientFrameViewAura::GetThemeFrameOverlayBitmap() const {
  ui::ThemeProvider* tp = GetThemeProvider();
  if (tp->HasCustomImage(IDR_THEME_FRAME_OVERLAY) &&
      browser_view()->IsBrowserTypeNormal() &&
      !browser_view()->IsOffTheRecord()) {
    return tp->GetBitmapNamed(ShouldPaintAsActive() ?
        IDR_THEME_FRAME_OVERLAY : IDR_THEME_FRAME_OVERLAY_INACTIVE);
  }
  return NULL;
}

SkBitmap* BrowserNonClientFrameViewAura::GetCustomBitmap(
    int bitmap_id,
    int fallback_bitmap_id) const {
  ui::ThemeProvider* tp = GetThemeProvider();
  if (tp->HasCustomImage(bitmap_id))
    return tp->GetBitmapNamed(bitmap_id);
  return tp->GetBitmapNamed(fallback_bitmap_id);
}
