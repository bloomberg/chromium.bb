// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/tabs/touch_tab.h"

#include "base/logging.h"
#include "chrome/browser/themes/theme_service.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skbitmap_operations.h"

static const int kLeftPadding = 16;
static const int kRightPadding = 15;
static const int kDropShadowHeight = 2;

// The size of the favicon touch area. This generally would be the same as
// kFaviconSize in ui/gfx/favicon_size.h
static const int kTouchTabIconSize = 32;

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
// TouchTab, protected:
const gfx::Rect& TouchTab::GetTitleBounds() const {
  return title_bounds_;
}

const gfx::Rect& TouchTab::GetIconBounds() const {
  return favicon_bounds_;
}

////////////////////////////////////////////////////////////////////////////////
// TouchTab, views::View overrides:

// We'll get selected via the mouse interactions with the TouchTabStrip.  There
// is no need to directly handle the mouse movements in the TouchTab.

bool TouchTab::OnMousePressed(const views::MouseEvent& event) {
  return false;
}

bool TouchTab::OnMouseDragged(const views::MouseEvent& event) {
  return false;
}

void TouchTab::OnMouseReleased(const views::MouseEvent& event) {
}

void TouchTab::OnPaint(gfx::Canvas* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumUnselectedSize().width() && !data().mini)
    return;

  PaintTabBackground(canvas);

  SkColor title_color = GetThemeProvider()->
      GetColor(IsSelected() ?
          ThemeService::COLOR_TAB_TEXT :
          ThemeService::COLOR_BACKGROUND_TAB_TEXT);

  PaintTitle(canvas, title_color);
  PaintIcon(canvas);
}

void TouchTab::Layout() {
  gfx::Rect local_bounds = GetContentsBounds();
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
  int offset = GetMirroredX() + background_offset_.x();
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
        (data().favicon.isNull() ? kFaviconSize : data().favicon.width());
  }

  int favicon_x = x;
  if (!data().favicon.isNull() && data().favicon.width() != kFaviconSize)
    favicon_x += (data().favicon.width() - kFaviconSize) / 2;

  if (data().network_state != TabRendererData::NETWORK_STATE_NONE) {
    ThemeProvider* tp = GetThemeProvider();
    SkBitmap frames(*tp->GetBitmapNamed(
        (data().network_state == TabRendererData::NETWORK_STATE_WAITING) ?
        IDR_THROBBER_WAITING : IDR_THROBBER));
    int image_size = frames.height();
    int image_offset = loading_animation_frame() * image_size;
    canvas->DrawBitmapInt(frames, image_offset, 0, image_size, image_size, x, y,
                          kTouchTabIconSize, kTouchTabIconSize, false);
  } else {
    canvas->Save();
    canvas->ClipRectInt(0, 0, width(), height());
    if (should_display_crashed_favicon()) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      SkBitmap crashed_favicon(*rb.GetBitmapNamed(IDR_SAD_FAVICON));
      canvas->DrawBitmapInt(crashed_favicon, 0, 0, crashed_favicon.width(),
          crashed_favicon.height(), x, y + favicon_hiding_offset(),
          kTouchTabIconSize, kTouchTabIconSize, true);
    } else {
      if (!data().favicon.isNull()) {

        if ((data().favicon.width() == kTouchTabIconSize) &&
            (data().favicon.height() == kTouchTabIconSize)) {
          canvas->DrawBitmapInt(data().favicon, 0, 0,
                                data().favicon.width(), data().favicon.height(),
                                x, y + favicon_hiding_offset(),
                                kTouchTabIconSize, kTouchTabIconSize, true);
        } else {
          // Draw a background around target touch area in case the favicon
          // is smaller than touch area (e.g www.google.com is 16x16 now)
          canvas->DrawRectInt(
              GetThemeProvider()->GetColor(
                  ThemeService::COLOR_BUTTON_BACKGROUND),
              x, y, kTouchTabIconSize, kTouchTabIconSize);

          // We center the image
          // TODO(saintlou): later request larger image from HistoryService
          canvas->DrawBitmapInt(data().favicon, 0, 0, data().favicon.width(),
              data().favicon.height(),
              x + ((kTouchTabIconSize - data().favicon.width()) / 2),
              y + ((kTouchTabIconSize - data().favicon.height()) / 2) +
                                                     favicon_hiding_offset(),
              data().favicon.width(), data().favicon.height(), true);
        }
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
