// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_aura.h"

#include "base/command_line.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_aura.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/theme_resources_standard.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/shadow_types.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/font.h"
#include "views/background.h"

namespace {

// The content left/right images have a shadow built into them.
const int kContentEdgeShadowThickness = 2;

// Background view to paint the gradient behind the back/forward/omnibox
// toolbar area.
class ToolbarBackground : public views::Background {
 public:
  explicit ToolbarBackground(BrowserView* browser_view);
  virtual ~ToolbarBackground();

  // views::Background overrides:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE;

 private:
  BrowserView* browser_view_;
  DISALLOW_COPY_AND_ASSIGN(ToolbarBackground);
};

ToolbarBackground::ToolbarBackground(BrowserView* browser_view)
    : browser_view_(browser_view) {
}

ToolbarBackground::~ToolbarBackground() {
}

void ToolbarBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  gfx::Rect toolbar_bounds = browser_view_->GetToolbarBounds();
  if (toolbar_bounds.IsEmpty())
    return;

  int x = toolbar_bounds.x();
  int w = toolbar_bounds.width();
  int y = toolbar_bounds.y();
  int h = toolbar_bounds.bottom();

  // Gross hack: We split the toolbar images into two pieces, since sometimes
  // (popup mode) the toolbar isn't tall enough to show the whole image.  The
  // split happens between the top shadow section and the bottom gradient
  // section so that we never break the gradient.
  int split_point = views::NonClientFrameView::kFrameShadowThickness * 2;
  int bottom_y = y + split_point;
  ui::ThemeProvider* tp = browser_view_->GetThemeProvider();
  SkBitmap* toolbar_left = tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER);
  int bottom_edge_height = std::min(toolbar_left->height(), h) - split_point;

  // Split our canvas out so we can mask out the corners of the toolbar
  // without masking out the frame.
  canvas->SaveLayerAlpha(
      255, gfx::Rect(x - views::NonClientFrameView::kClientEdgeThickness,
                     y,
                     w + views::NonClientFrameView::kClientEdgeThickness * 3,
                     h));
  canvas->GetSkCanvas()->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);

  SkColor theme_toolbar_color = tp->GetColor(ThemeService::COLOR_TOOLBAR);
  canvas->FillRect(theme_toolbar_color,
                   gfx::Rect(x, bottom_y, w, bottom_edge_height));

  // Tile the toolbar image starting at the frame edge on the left and where the
  // horizontal tabstrip is (or would be) on the top.
  SkBitmap* theme_toolbar = tp->GetBitmapNamed(IDR_THEME_TOOLBAR);
  canvas->TileImageInt(*theme_toolbar, x,
                       bottom_y, x,
                       bottom_y, w, theme_toolbar->height());

  // Draw rounded corners for the tab.
  SkBitmap* toolbar_left_mask =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER_MASK);
  SkBitmap* toolbar_right_mask =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_RIGHT_CORNER_MASK);

  // We mask out the corners by using the DestinationIn transfer mode,
  // which keeps the RGB pixels from the destination and the alpha from
  // the source.
  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kDstIn_Mode);

  // Mask the left edge.
  int left_x = x - kContentEdgeShadowThickness;
  canvas->DrawBitmapInt(*toolbar_left_mask, 0, 0, toolbar_left_mask->width(),
                        split_point, left_x, y, toolbar_left_mask->width(),
                        split_point, false, paint);
  canvas->DrawBitmapInt(*toolbar_left_mask, 0,
      toolbar_left_mask->height() - bottom_edge_height,
      toolbar_left_mask->width(), bottom_edge_height, left_x, bottom_y,
      toolbar_left_mask->width(), bottom_edge_height, false, paint);

  // Mask the right edge.
  int right_x =
      x + w - toolbar_right_mask->width() + kContentEdgeShadowThickness;
  canvas->DrawBitmapInt(*toolbar_right_mask, 0, 0, toolbar_right_mask->width(),
                        split_point, right_x, y, toolbar_right_mask->width(),
                        split_point, false, paint);
  canvas->DrawBitmapInt(*toolbar_right_mask, 0,
      toolbar_right_mask->height() - bottom_edge_height,
      toolbar_right_mask->width(), bottom_edge_height, right_x, bottom_y,
      toolbar_right_mask->width(), bottom_edge_height, false, paint);
  canvas->Restore();

  canvas->DrawBitmapInt(*toolbar_left, 0, 0, toolbar_left->width(), split_point,
                        left_x, y, toolbar_left->width(), split_point, false);
  canvas->DrawBitmapInt(*toolbar_left, 0,
      toolbar_left->height() - bottom_edge_height, toolbar_left->width(),
      bottom_edge_height, left_x, bottom_y, toolbar_left->width(),
      bottom_edge_height, false);

  SkBitmap* toolbar_center =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_CENTER);
  canvas->TileImageInt(*toolbar_center, 0, 0, left_x + toolbar_left->width(),
      y, right_x - (left_x + toolbar_left->width()),
      split_point);

  SkBitmap* toolbar_right = tp->GetBitmapNamed(IDR_CONTENT_TOP_RIGHT_CORNER);
  canvas->DrawBitmapInt(*toolbar_right, 0, 0, toolbar_right->width(),
      split_point, right_x, y, toolbar_right->width(), split_point, false);
  canvas->DrawBitmapInt(*toolbar_right, 0,
      toolbar_right->height() - bottom_edge_height, toolbar_right->width(),
      bottom_edge_height, right_x, bottom_y, toolbar_right->width(),
      bottom_edge_height, false);

  // Draw the content/toolbar separator.
  canvas->FillRect(
      ResourceBundle::toolbar_separator_color,
      gfx::Rect(
          x + views::NonClientFrameView::kClientEdgeThickness,
          toolbar_bounds.bottom() - views::NonClientFrameView::kClientEdgeThickness,
          w - (2 * views::NonClientFrameView::kClientEdgeThickness),
          views::NonClientFrameView::kClientEdgeThickness));
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAura, public:

BrowserFrameAura::BrowserFrameAura(BrowserFrame* browser_frame,
                                   BrowserView* browser_view)
    : views::NativeWidgetAura(browser_frame),
      browser_view_(browser_view),
      browser_frame_(browser_frame) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAuraWindows)) {
    // Aura paints layers behind this view, so this must be a layer also.
    // TODO: see if we can avoid this, layers are expensive.
    browser_view_->SetPaintToLayer(true);
    browser_view_->layer()->SetFillsBoundsOpaquely(false);
    // Background only needed for Aura-style windows.
    browser_view_->set_background(new ToolbarBackground(browser_view));
  } else {
    GetNativeWindow()->SetIntProperty(aura::kShadowTypeKey,
                                      aura::SHADOW_TYPE_RECTANGULAR);
  }
}

BrowserFrameAura::~BrowserFrameAura() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAura, views::NativeWidgetAura overrides:

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameAura, NativeBrowserFrame implementation:

views::NativeWidget* BrowserFrameAura::AsNativeWidget() {
  return this;
}

const views::NativeWidget* BrowserFrameAura::AsNativeWidget() const {
  return this;
}

int BrowserFrameAura::GetMinimizeButtonOffset() const {
  return 0;
}

void BrowserFrameAura::TabStripDisplayModeChanged() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

// static
const gfx::Font& BrowserFrame::GetTitleFont() {
  static gfx::Font* title_font = new gfx::Font;
  return *title_font;
}

////////////////////////////////////////////////////////////////////////////////
// NativeBrowserFrame, public:

// static
NativeBrowserFrame* NativeBrowserFrame::CreateNativeBrowserFrame(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  return new BrowserFrameAura(browser_frame, browser_view);
}
