// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"

namespace {

// How far to inset the tabstrip from the sides of the window.
const int kTabstripTopInset = 8;
const int kTabstripLeftInset = 70;  // Make room for window control buttons.
const int kTabstripRightInset = 0;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, public:

BrowserNonClientFrameViewMac::BrowserNonClientFrameViewMac(
    BrowserFrame* frame, BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
}

BrowserNonClientFrameViewMac::~BrowserNonClientFrameViewMac() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, BrowserNonClientFrameView implementation:

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  DCHECK(tabstrip);
  gfx::Rect bounds = gfx::Rect(0, kTabstripTopInset,
                               width(), tabstrip->GetPreferredSize().height());
  bounds.Inset(kTabstripLeftInset, 0, kTabstripRightInset, 0);
  return bounds;
}

int BrowserNonClientFrameViewMac::GetTopInset(bool restored) const {
  return browser_view()->IsTabStripVisible() ? kTabstripTopInset : 0;
}

int BrowserNonClientFrameViewMac::GetThemeBackgroundXInset() const {
  return 0;
}

void BrowserNonClientFrameViewMac::UpdateThrobber(bool running) {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::NonClientFrameView implementation:

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int BrowserNonClientFrameViewMac::NonClientHitTest(const gfx::Point& point) {
  int component = frame()->client_view()->NonClientHitTest(point);

  // BrowserView::NonClientHitTest will return HTNOWHERE for points that hit
  // the native title bar. On Mac, we need to explicitly return HTCAPTION for
  // those points.
  if (component == HTNOWHERE && bounds().Contains(point))
    return HTCAPTION;

  return component;
}

void BrowserNonClientFrameViewMac::GetWindowMask(const gfx::Size& size,
                                                 gfx::Path* window_mask) {
}

void BrowserNonClientFrameViewMac::ResetWindowControls() {
}

void BrowserNonClientFrameViewMac::UpdateWindowIcon() {
}

void BrowserNonClientFrameViewMac::UpdateWindowTitle() {
}

void BrowserNonClientFrameViewMac::SizeConstraintsChanged() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::View implementation:

gfx::Size BrowserNonClientFrameViewMac::GetMinimumSize() const {
  return browser_view()->GetMinimumSize();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, protected:

// views::View:
void BrowserNonClientFrameViewMac::OnPaint(gfx::Canvas* canvas) {
  if (!browser_view()->IsBrowserTypeNormal())
    return;

  canvas->DrawColor(GetFrameColor());

  if (!GetThemeProvider()->UsingSystemTheme())
    PaintThemedFrame(canvas);

  if (browser_view()->IsToolbarVisible())
    PaintToolbarBackground(canvas);
}

// BrowserNonClientFrameView:
void BrowserNonClientFrameViewMac::UpdateProfileIcons() {
  NOTIMPLEMENTED();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, private:

void BrowserNonClientFrameViewMac::PaintThemedFrame(gfx::Canvas* canvas) {
  gfx::ImageSkia image = GetFrameImage();
  canvas->TileImageInt(image, 0, 0, width(), image.height());
  gfx::ImageSkia overlay = GetFrameOverlayImage();
  canvas->TileImageInt(overlay, 0, 0, width(), overlay.height());
}

void BrowserNonClientFrameViewMac::PaintToolbarBackground(gfx::Canvas* canvas) {
  gfx::Rect bounds(browser_view()->GetToolbarBounds());
  if (bounds.IsEmpty())
    return;

  const ui::ThemeProvider* tp = GetThemeProvider();
  gfx::ImageSkia* border = tp->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_TOP);

  const int x = bounds.x();
  const int y = bounds.y() - border->height();
  const int w = bounds.width();
  const int h = bounds.height() + border->height();

  // The tabstrip border image height is 2*scale pixels, but only the bottom 2
  // pixels contain the actual border (the rest is transparent). We can't draw
  // the toolbar image below this transparent upper area when scale > 1.
  const int fill_y = y + canvas->image_scale() - 1;
  const int fill_height = bounds.bottom() - fill_y;

  // Draw the toolbar fill.
  canvas->TileImageInt(*tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR),
                       x + GetThemeBackgroundXInset(),
                       fill_y - GetTopInset(false), x, fill_y, w, fill_height);

  // Draw the tabstrip/toolbar separator.
  canvas->TileImageInt(*border, 0, 0, x, y, w, border->height());

  // Draw the content/toolbar separator.
  canvas->FillRect(
      gfx::Rect(x, y + h - kClientEdgeThickness, w, kClientEdgeThickness),
      ThemeProperties::GetDefaultColor(
          ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR,
          browser_view()->IsIncognito()));
}
