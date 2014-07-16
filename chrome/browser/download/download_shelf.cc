// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include "chrome/browser/download/download_shelf.h"

#include <cmath>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_started_animation.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"

using content::DownloadItem;

namespace {

// Delay before we show a transient download.
const int64 kDownloadShowDelayInSeconds = 2;

// Get the opacity based on |animation_progress|, with values in [0.0, 1.0].
// Range of return value is [0, 255].
int GetOpacity(double animation_progress) {
  DCHECK(animation_progress >= 0 && animation_progress <= 1);

  // How many times to cycle the complete animation. This should be an odd
  // number so that the animation ends faded out.
  static const int kCompleteAnimationCycles = 5;
  double temp = animation_progress * kCompleteAnimationCycles * M_PI + M_PI_2;
  temp = sin(temp) / 2 + 0.5;
  return static_cast<int>(255.0 * temp);
}

} // namespace

DownloadShelf::DownloadShelf()
    : should_show_on_unhide_(false),
      is_hidden_(false),
      weak_ptr_factory_(this) {
}

DownloadShelf::~DownloadShelf() {}

// static
int DownloadShelf::GetBigProgressIconSize() {
  static int big_progress_icon_size = 0;
  if (big_progress_icon_size == 0) {
    base::string16 locale_size_str =
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_BIG_PROGRESS_SIZE);
    bool rc = base::StringToInt(locale_size_str, &big_progress_icon_size);
    if (!rc || big_progress_icon_size < kBigProgressIconSize) {
      NOTREACHED();
      big_progress_icon_size = kBigProgressIconSize;
    }
  }

  return big_progress_icon_size;
}

// static
int DownloadShelf::GetBigProgressIconOffset() {
  return (GetBigProgressIconSize() - kBigIconSize) / 2;
}

// Download progress painting --------------------------------------------------

// Common images used for download progress animations. We load them once the
// first time we do a progress paint, then reuse them as they are always the
// same.
gfx::ImageSkia* g_foreground_16 = NULL;
gfx::ImageSkia* g_background_16 = NULL;
gfx::ImageSkia* g_foreground_32 = NULL;
gfx::ImageSkia* g_background_32 = NULL;

// static
void DownloadShelf::PaintCustomDownloadProgress(
    gfx::Canvas* canvas,
    const gfx::ImageSkia& background_image,
    const gfx::ImageSkia& foreground_image,
    int image_size,
    const gfx::Rect& bounds,
    int start_angle,
    int percent_done) {
  // Draw the background progress image.
  canvas->DrawImageInt(background_image,
                       bounds.x(),
                       bounds.y());

  // Layer the foreground progress image in an arc proportional to the download
  // progress. The arc grows clockwise, starting in the midnight position, as
  // the download progresses. However, if the download does not have known total
  // size (the server didn't give us one), then we just spin an arc around until
  // we're done.
  float sweep_angle = 0.0;
  float start_pos = static_cast<float>(kStartAngleDegrees);
  if (percent_done < 0) {
    sweep_angle = kUnknownAngleDegrees;
    start_pos = static_cast<float>(start_angle);
  } else if (percent_done > 0) {
    sweep_angle = static_cast<float>(kMaxDegrees / 100.0 * percent_done);
  }

  // Set up an arc clipping region for the foreground image. Don't bother using
  // a clipping region if it would round to 360 (really 0) degrees, since that
  // would eliminate the foreground completely and be quite confusing (it would
  // look like 0% complete when it should be almost 100%).
  canvas->Save();
  if (sweep_angle < static_cast<float>(kMaxDegrees - 1)) {
    SkRect oval;
    oval.set(SkIntToScalar(bounds.x()),
             SkIntToScalar(bounds.y()),
             SkIntToScalar(bounds.x() + image_size),
             SkIntToScalar(bounds.y() + image_size));
    SkPath path;
    path.arcTo(oval,
               SkFloatToScalar(start_pos),
               SkFloatToScalar(sweep_angle), false);
    path.lineTo(SkIntToScalar(bounds.x() + image_size / 2),
                SkIntToScalar(bounds.y() + image_size / 2));

    // gfx::Canvas::ClipPath does not provide for anti-aliasing.
    canvas->sk_canvas()->clipPath(path, SkRegion::kIntersect_Op, true);
  }

  canvas->DrawImageInt(foreground_image,
                       bounds.x(),
                       bounds.y());
  canvas->Restore();
}

// static
void DownloadShelf::PaintDownloadProgress(
    gfx::Canvas* canvas,
    const BoundsAdjusterCallback& rtl_mirror,
    int origin_x,
    int origin_y,
    int start_angle,
    int percent_done,
    PaintDownloadProgressSize size) {
  // Load up our common images.
  if (!g_background_16) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    g_foreground_16 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_16);
    g_background_16 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_BACKGROUND_16);
    g_foreground_32 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_32);
    g_background_32 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_BACKGROUND_32);
    DCHECK_EQ(g_foreground_16->width(), g_background_16->width());
    DCHECK_EQ(g_foreground_16->height(), g_background_16->height());
    DCHECK_EQ(g_foreground_32->width(), g_background_32->width());
    DCHECK_EQ(g_foreground_32->height(), g_background_32->height());
  }

  gfx::ImageSkia* background =
      (size == BIG) ? g_background_32 : g_background_16;
  gfx::ImageSkia* foreground =
      (size == BIG) ? g_foreground_32 : g_foreground_16;

  const int kProgressIconSize =
      (size == BIG) ? kBigProgressIconSize : kSmallProgressIconSize;

  // We start by storing the bounds of the images so that it is easy to mirror
  // the bounds if the UI layout is RTL.
  gfx::Rect bounds(origin_x, origin_y,
                   background->width(), background->height());

  // Mirror the positions if necessary.
  rtl_mirror.Run(&bounds);

  // Draw the background progress image.
  canvas->DrawImageInt(*background,
                       bounds.x(),
                       bounds.y());

  PaintCustomDownloadProgress(canvas,
                              *background,
                              *foreground,
                              kProgressIconSize,
                              bounds,
                              start_angle,
                              percent_done);
}

// static
void DownloadShelf::PaintDownloadComplete(
    gfx::Canvas* canvas,
    const BoundsAdjusterCallback& rtl_mirror,
    int origin_x,
    int origin_y,
    double animation_progress,
    PaintDownloadProgressSize size) {
  // Load up our common images.
  if (!g_foreground_16) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    g_foreground_16 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_16);
    g_foreground_32 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_32);
  }

  gfx::ImageSkia* complete = (size == BIG) ? g_foreground_32 : g_foreground_16;

  gfx::Rect complete_bounds(origin_x, origin_y,
                            complete->width(), complete->height());
  // Mirror the positions if necessary.
  rtl_mirror.Run(&complete_bounds);

  // Start at full opacity, then loop back and forth five times before ending
  // at zero opacity.
  canvas->DrawImageInt(*complete, complete_bounds.x(), complete_bounds.y(),
                       GetOpacity(animation_progress));
}

// static
void DownloadShelf::PaintDownloadInterrupted(
    gfx::Canvas* canvas,
    const BoundsAdjusterCallback& rtl_mirror,
    int origin_x,
    int origin_y,
    double animation_progress,
    PaintDownloadProgressSize size) {
  // Load up our common images.
  if (!g_foreground_16) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    g_foreground_16 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_16);
    g_foreground_32 = rb.GetImageSkiaNamed(IDR_DOWNLOAD_PROGRESS_FOREGROUND_32);
  }

  gfx::ImageSkia* complete = (size == BIG) ? g_foreground_32 : g_foreground_16;

  gfx::Rect complete_bounds(origin_x, origin_y,
                            complete->width(), complete->height());
  // Mirror the positions if necessary.
  rtl_mirror.Run(&complete_bounds);

  // Start at zero opacity, then loop back and forth five times before ending
  // at full opacity.
  canvas->DrawImageInt(*complete, complete_bounds.x(), complete_bounds.y(),
                       GetOpacity(1.0 - animation_progress));
}

void DownloadShelf::AddDownload(DownloadItem* download) {
  DCHECK(download);
  if (DownloadItemModel(download).ShouldRemoveFromShelfWhenComplete()) {
    // If we are going to remove the download from the shelf upon completion,
    // wait a few seconds to see if it completes quickly. If it's a small
    // download, then the user won't have time to interact with it.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DownloadShelf::ShowDownloadById,
                   weak_ptr_factory_.GetWeakPtr(),
                   download->GetId()),
        GetTransientDownloadShowDelay());
  } else {
    ShowDownload(download);
  }
}

void DownloadShelf::Show() {
  if (is_hidden_) {
    should_show_on_unhide_ = true;
    return;
  }
  DoShow();
}

void DownloadShelf::Close(CloseReason reason) {
  if (is_hidden_) {
    should_show_on_unhide_ = false;
    return;
  }
  DoClose(reason);
}

void DownloadShelf::Hide() {
  if (is_hidden_)
    return;
  is_hidden_ = true;
  if (IsShowing()) {
    should_show_on_unhide_ = true;
    DoClose(AUTOMATIC);
  }
}

void DownloadShelf::Unhide() {
  if (!is_hidden_)
    return;
  is_hidden_ = false;
  if (should_show_on_unhide_) {
    should_show_on_unhide_ = false;
    DoShow();
  }
}

base::TimeDelta DownloadShelf::GetTransientDownloadShowDelay() {
  return base::TimeDelta::FromSeconds(kDownloadShowDelayInSeconds);
}

content::DownloadManager* DownloadShelf::GetDownloadManager() {
  return content::BrowserContext::GetDownloadManager(browser()->profile());
}

void DownloadShelf::ShowDownload(DownloadItem* download) {
  if (download->GetState() == DownloadItem::COMPLETE &&
      DownloadItemModel(download).ShouldRemoveFromShelfWhenComplete())
    return;
  if (!DownloadServiceFactory::GetForBrowserContext(
        download->GetBrowserContext())->IsShelfEnabled())
    return;

  if (is_hidden_)
    Unhide();
  Show();
  DoAddDownload(download);

  // browser() can be NULL for tests.
  if (!browser())
    return;

  // Show the download started animation if:
  // - Download started animation is enabled for this download. It is disabled
  //   for "Save As" downloads and extension installs, for example.
  // - The browser has an active visible WebContents. (browser isn't minimized,
  //   or running under a test etc.)
  // - Rich animations are enabled.
  content::WebContents* shelf_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (DownloadItemModel(download).ShouldShowDownloadStartedAnimation() &&
      shelf_tab &&
      platform_util::IsVisible(shelf_tab->GetNativeView()) &&
      gfx::Animation::ShouldRenderRichAnimation()) {
    DownloadStartedAnimation::Show(shelf_tab);
  }
}

void DownloadShelf::ShowDownloadById(int32 download_id) {
  content::DownloadManager* download_manager = GetDownloadManager();
  if (!download_manager)
    return;

  DownloadItem* download = download_manager->GetDownload(download_id);
  if (!download)
    return;

  ShowDownload(download);
}
