// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_icon_view.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#endif

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/resources/grit/ui_resources.h"

#if defined(OS_WIN)
#include "chrome/browser/app_icon_win.h"
#include "ui/gfx/icon_util.h"
#endif

static bool g_initialized = false;
static gfx::ImageSkia* g_default_favicon = NULL;

// static
void TabIconView::InitializeIfNeeded() {
  if (!g_initialized) {
    g_initialized = true;

#if defined(OS_WIN)
    // The default window icon is the application icon, not the default
    // favicon.
    HICON app_icon = GetAppIcon();
    scoped_ptr<SkBitmap> bitmap(
        IconUtil::CreateSkBitmapFromHICON(app_icon, gfx::Size(16, 16)));
    g_default_favicon = new gfx::ImageSkia(gfx::ImageSkiaRep(*bitmap, 1.0f));
    DestroyIcon(app_icon);
#else
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    g_default_favicon = rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_16);
#endif
  }
}

TabIconView::TabIconView(chrome::TabIconViewModel* model,
                         views::MenuButtonListener* listener)
    : views::MenuButton(NULL, base::string16(), listener, false),
      model_(model),
      throbber_running_(false),
      is_light_(false),
      throbber_frame_(0) {
  InitializeIfNeeded();
}

TabIconView::~TabIconView() {
}

void TabIconView::Update() {
  static bool initialized = false;
  static int throbber_frame_count = 0;
  if (!initialized) {
    initialized = true;
    gfx::ImageSkia throbber(*ui::ResourceBundle::GetSharedInstance().
        GetImageSkiaNamed(IDR_THROBBER));
    throbber_frame_count = throbber.width() / throbber.height();
  }

  if (throbber_running_) {
    // We think the tab is loading.
    if (!model_->ShouldTabIconViewAnimate()) {
      // Woops, tab is invalid or not loading, reset our status and schedule
      // a paint.
      throbber_running_ = false;
      SchedulePaint();
    } else {
      // The tab is still loading, increment the frame.
      throbber_frame_ = (throbber_frame_ + 1) % throbber_frame_count;
      SchedulePaint();
    }
  } else if (model_->ShouldTabIconViewAnimate()) {
    // We didn't think we were loading, but the tab is loading. Reset the
    // frame and status and schedule a paint.
    throbber_running_ = true;
    throbber_frame_ = 0;
    SchedulePaint();
  }
}

void TabIconView::PaintThrobber(gfx::Canvas* canvas) {
  gfx::ImageSkia throbber(*GetThemeProvider()->GetImageSkiaNamed(
      is_light_ ? IDR_THROBBER_LIGHT : IDR_THROBBER));
  int image_size = throbber.height();
  PaintIcon(canvas, throbber, throbber_frame_ * image_size, 0, image_size,
            image_size, false);
}

void TabIconView::PaintFavicon(gfx::Canvas* canvas,
                               const gfx::ImageSkia& image) {
  PaintIcon(canvas, image, 0, 0, image.width(), image.height(), true);
}

void TabIconView::PaintIcon(gfx::Canvas* canvas,
                            const gfx::ImageSkia& image,
                            int src_x,
                            int src_y,
                            int src_w,
                            int src_h,
                            bool filter) {
  // For source images smaller than the favicon square, scale them as if they
  // were padded to fit the favicon square, so we don't blow up tiny favicons
  // into larger or nonproportional results.
  float float_src_w = static_cast<float>(src_w);
  float float_src_h = static_cast<float>(src_h);
  float scalable_w, scalable_h;
  if (src_w <= gfx::kFaviconSize && src_h <= gfx::kFaviconSize) {
    scalable_w = scalable_h = gfx::kFaviconSize;
  } else {
    scalable_w = float_src_w;
    scalable_h = float_src_h;
  }

  // Scale proportionately.
  float scale = std::min(static_cast<float>(width()) / scalable_w,
                         static_cast<float>(height()) / scalable_h);
  int dest_w = static_cast<int>(float_src_w * scale);
  int dest_h = static_cast<int>(float_src_h * scale);

  // Center the scaled image.
  canvas->DrawImageInt(image, src_x, src_y, src_w, src_h,
                       (width() - dest_w) / 2, (height() - dest_h) / 2, dest_w,
                       dest_h, filter);
}

void TabIconView::OnPaint(gfx::Canvas* canvas) {
  bool rendered = false;

  if (throbber_running_) {
    rendered = true;
    PaintThrobber(canvas);
  } else {
    gfx::ImageSkia favicon = model_->GetFaviconForTabIconView();
    if (!favicon.isNull()) {
      rendered = true;
      PaintFavicon(canvas, favicon);
    }
  }

  if (!rendered)
    PaintFavicon(canvas, *g_default_favicon);
}

gfx::Size TabIconView::GetPreferredSize() const {
  return gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize);
}
