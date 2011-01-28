// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/tabs/touch_tab.h"

#include "chrome/browser/themes/browser_theme_provider.h"
#include "gfx/canvas_skia.h"
#include "gfx/favicon_size.h"
#include "gfx/path.h"
#include "gfx/skbitmap_operations.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

static const int kLeftPadding = 16;
static const int kRightPadding = 15;
static const int kDropShadowHeight = 2;
static const int kTouchFaviconSize = 32;

TouchTab::TouchTabImage TouchTab::tab_alpha = {0};
TouchTab::TouchTabImage TouchTab::tab_active = {0};
TouchTab::TouchTabImage TouchTab::tab_inactive = {0};

////////////////////////////////////////////////////////////////////////////////
// TouchTab, public:

TouchTab::TouchTab(TabController* controller)
    : BaseTab(controller) {
  InitTabResources();
}

TouchTab::~TouchTab() {
}

// static
gfx::Size TouchTab::GetMinimumUnselectedSize() {
  InitTabResources();

  gfx::Size minimum_size;
  minimum_size.set_width(kLeftPadding + kRightPadding);
  minimum_size.set_height(32);
  return minimum_size;
}

////////////////////////////////////////////////////////////////////////////////
// TouchTab, views::View overrides:

void TouchTab::Paint(gfx::Canvas* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumUnselectedSize().width() && !data().mini)
    return;

  PaintTabBackground(canvas);

  SkColor title_color = GetThemeProvider()->
      GetColor(IsSelected() ?
          BrowserThemeProvider::COLOR_TAB_TEXT :
          BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT);

  PaintTitle(canvas, title_color);
  PaintIcon(canvas);
}

void TouchTab::Layout() {
  gfx::Rect local_bounds = GetLocalBounds(false);
  TouchTabImage* tab_image = &tab_active;
  TouchTabImage* alpha = &tab_alpha;
  int image_height = alpha->image_l->height();
  int x_base = tab_image->image_l->width();
  int y_base = height() - image_height;
  int center_width = width() - tab_image->l_width - tab_image->r_width;
  if (center_width < 0)
    center_width = 0;
  title_bounds_ = gfx::Rect(x_base, y_base, center_width, image_height);
  favicon_bounds_ = local_bounds;
}

bool TouchTab::HasHitTestMask() const {
  return true;
}

void TouchTab::GetHitTestMask(gfx::Path* path) const {
  DCHECK(path);

  SkScalar h = SkIntToScalar(height());
  SkScalar w = SkIntToScalar(width());

  path->moveTo(0, h);
  path->lineTo(0, 0);
  path->lineTo(w, 0);
  path->lineTo(w, h);
  path->lineTo(0, h);
  path->close();
}

////////////////////////////////////////////////////////////////////////////////
// TouchTab, private:

void TouchTab::PaintTabBackground(gfx::Canvas* canvas) {
  if (IsSelected()) {
    PaintActiveTabBackground(canvas);
  }
}

void TouchTab::PaintActiveTabBackground(gfx::Canvas* canvas) {
  int offset = GetX(views::View::APPLY_MIRRORING_TRANSFORMATION) +
      background_offset_.x();
  ThemeProvider* tp = GetThemeProvider();
  if (!tp)
    NOTREACHED() << "Unable to get theme provider";

  SkBitmap* tab_bg = GetThemeProvider()->GetBitmapNamed(IDR_THEME_TOOLBAR);

  TouchTabImage* tab_image = &tab_active;
  TouchTabImage* alpha = &tab_alpha;

  // Draw left edge.
  int image_height = alpha->image_l->height();
  int y_base = height() - image_height;
  SkBitmap tab_l = SkBitmapOperations::CreateTiledBitmap(
      *tab_bg, offset, 0, tab_image->l_width, image_height);
  SkBitmap theme_l =
      SkBitmapOperations::CreateMaskedBitmap(tab_l, *alpha->image_l);
  canvas->DrawBitmapInt(theme_l, 0, y_base);

  // Draw right edge.
  SkBitmap tab_r = SkBitmapOperations::CreateTiledBitmap(*tab_bg,
      offset + width() - tab_image->r_width, 0,
      tab_image->r_width, image_height);
  SkBitmap theme_r =
      SkBitmapOperations::CreateMaskedBitmap(tab_r, *alpha->image_r);
  canvas->DrawBitmapInt(theme_r, width() - tab_image->r_width, y_base);

  // Draw center.  Instead of masking out the top portion we simply skip over it
  // by incrementing by kDropShadowHeight, since it's a simple rectangle.
  canvas->TileImageInt(*tab_bg,
     offset + tab_image->l_width,
     kDropShadowHeight + tab_image->y_offset,
     tab_image->l_width,
     y_base + kDropShadowHeight + tab_image->y_offset,
     width() - tab_image->l_width - tab_image->r_width,
     height() - kDropShadowHeight - tab_image->y_offset);

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawBitmapInt(*tab_image->image_l, 0, y_base);
  canvas->TileImageInt(*tab_image->image_c, tab_image->l_width, y_base,
      width() - tab_image->l_width - tab_image->r_width, image_height);
  canvas->DrawBitmapInt(*tab_image->image_r, width() - tab_image->r_width,
                        y_base);
}

void TouchTab::PaintIcon(gfx::Canvas* canvas) {
  // TODO(wyck): use thumbnailer to get better page images
  int x = favicon_bounds_.x();
  int y = favicon_bounds_.y();

  TouchTabImage* tab_image = &tab_active;
  int x_base = tab_image->image_l->width();

  x += x_base;

  if (base::i18n::IsRTL()) {
    x = width() - x -
        (data().favicon.isNull() ? kFavIconSize : data().favicon.width());
  }

  int favicon_x = x;
  if (!data().favicon.isNull() && data().favicon.width() != kFavIconSize)
    favicon_x += (data().favicon.width() - kFavIconSize) / 2;

  if (data().network_state != TabRendererData::NETWORK_STATE_NONE) {
    ThemeProvider* tp = GetThemeProvider();
    SkBitmap frames(*tp->GetBitmapNamed(
        (data().network_state == TabRendererData::NETWORK_STATE_WAITING) ?
        IDR_THROBBER_WAITING : IDR_THROBBER));
    int image_size = frames.height();
    int image_offset = loading_animation_frame() * image_size;
    canvas->DrawBitmapInt(frames, image_offset, 0, image_size, image_size, x, y,
                          kTouchFaviconSize, kTouchFaviconSize, false);
  } else {
    canvas->Save();
    canvas->ClipRectInt(0, 0, width(), height());
    if (should_display_crashed_favicon()) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      SkBitmap crashed_fav_icon(*rb.GetBitmapNamed(IDR_SAD_FAVICON));
      canvas->DrawBitmapInt(crashed_fav_icon, 0, 0, crashed_fav_icon.width(),
          crashed_fav_icon.height(), x, y + fav_icon_hiding_offset(),
          kTouchFaviconSize, kTouchFaviconSize, true);
    } else {
      if (!data().favicon.isNull()) {
        canvas->DrawBitmapInt(data().favicon, 0, 0,
                              data().favicon.width(), data().favicon.height(),
                              x, y + fav_icon_hiding_offset(),
                              kTouchFaviconSize, kTouchFaviconSize, true);
      }
    }
    canvas->Restore();
  }
}

// static
void TouchTab::InitTabResources() {
  static bool initialized = false;
  if (initialized)
    return;

  initialized = true;

  // Load the tab images once now, and maybe again later if the theme changes.
  LoadTabImages();
}


// static
void TouchTab::LoadTabImages() {
  // We're not letting people override tab images just yet.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  tab_alpha.image_l = rb.GetBitmapNamed(IDR_TAB_ALPHA_LEFT);
  tab_alpha.image_r = rb.GetBitmapNamed(IDR_TAB_ALPHA_RIGHT);

  tab_active.image_l = rb.GetBitmapNamed(IDR_TAB_ACTIVE_LEFT);
  tab_active.image_c = rb.GetBitmapNamed(IDR_TAB_ACTIVE_CENTER);
  tab_active.image_r = rb.GetBitmapNamed(IDR_TAB_ACTIVE_RIGHT);
  tab_active.l_width = tab_active.image_l->width();
  tab_active.r_width = tab_active.image_r->width();

  tab_inactive.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT);
  tab_inactive.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER);
  tab_inactive.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT);
  tab_inactive.l_width = tab_inactive.image_l->width();
  tab_inactive.r_width = tab_inactive.image_r->width();
}
