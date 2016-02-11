// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/tab_desktop_media_list.h"

#include "base/hash.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::DesktopMediaID;

namespace {

// Returns a hash of a favicon to detect when the favicon of media source has
// changed.
uint32_t GetImageHash(const gfx::Image& favicon) {
  SkBitmap bitmap = favicon.AsBitmap();
  bitmap.lockPixels();
  uint32_t value =
      base::Hash(reinterpret_cast<char*>(bitmap.getPixels()), bitmap.getSize());
  bitmap.unlockPixels();

  return value;
}

gfx::ImageSkia CreateEnlargedFaviconImage(gfx::Size size,
                                          const gfx::ImageSkia& favicon) {
  DCHECK_GE(size.width(), 20);
  DCHECK_GE(size.height(), 20);

  // Create a bitmap.
  SkBitmap result;
  result.allocN32Pixels(size.width(), size.height(), true);
  SkCanvas canvas(result);

  // White background.
  canvas.drawARGB(255, 255, 255, 255);

  // Black border.
  const int thickness = 5;
  SkPaint paint;
  paint.setARGB(255, 0, 0, 0);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(thickness);
  canvas.drawRectCoords(thickness,                    // left
                        thickness,                    // top
                        result.width() - thickness,   // right
                        result.height() - thickness,  // bottom
                        paint);

  // Draw a scaled favicon image into the center of result image to take up to
  // a fraction of result image.
  const double fraction = 0.75;
  gfx::Size target_size(result.width() * fraction, result.height() * fraction);
  gfx::Size scaled_favicon_size =
      media::ScaleSizeToFitWithinTarget(favicon.size(), target_size);
  gfx::Rect center_rect(result.width(), result.height());
  center_rect.ClampToCenteredSize(scaled_favicon_size);
  SkRect dest_rect =
      SkRect::MakeLTRB(center_rect.x(), center_rect.y(), center_rect.right(),
                       center_rect.bottom());
  const scoped_ptr<SkImage> bitmap(SkImage::NewFromBitmap(*favicon.bitmap()));
  canvas.drawImageRect(bitmap.get(), dest_rect, nullptr);

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}

// Update the list once per second.
const int kDefaultUpdatePeriod = 1000;

}  // namespace

TabDesktopMediaList::TabDesktopMediaList()
    : DesktopMediaListBase(
          base::TimeDelta::FromMilliseconds(kDefaultUpdatePeriod)),
      weak_factory_(this) {
  base::SequencedWorkerPool* worker_pool = BrowserThread::GetBlockingPool();
  thumbnail_task_runner_ =
      worker_pool->GetSequencedTaskRunner(worker_pool->GetSequenceToken());
}

TabDesktopMediaList::~TabDesktopMediaList() {}

void TabDesktopMediaList::Refresh() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  std::vector<Browser*> browsers;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() ==
        profile->GetOriginalProfile()) {
      browsers.push_back(browser);
    }
  }

  ImageHashesMap new_favicon_hashes;
  std::vector<SourceDescription> sources;
  std::map<base::TimeTicks, SourceDescription> tab_map;
  std::vector<std::pair<DesktopMediaID, gfx::ImageSkia>> favicon_pairs;

  // Enumerate all tabs with their titles and favicons for a user profile.
  for (auto browser : browsers) {
    const TabStripModel* tab_strip_model = browser->tab_strip_model();
    DCHECK(tab_strip_model);

    for (int i = 0; i < tab_strip_model->count(); i++) {
      // Create id for tab.
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
      DCHECK(contents);
      content::RenderFrameHost* main_frame = contents->GetMainFrame();
      DCHECK(main_frame);
      DesktopMediaID media_id(
          DesktopMediaID::TYPE_WEB_CONTENTS, DesktopMediaID::kNullId,
          content::WebContentsMediaCaptureId(main_frame->GetProcess()->GetID(),
                                             main_frame->GetRoutingID()));

      // Create display tab title.
      const base::string16 title = l10n_util::GetStringFUTF16(
          IDS_DESKTOP_MEDIA_PICKER_CHROME_TAB_TITLE, contents->GetTitle());

      // Get tab's last active time stamp.
      const base::TimeTicks t = contents->GetLastActiveTime();
      tab_map.insert(std::make_pair(t, SourceDescription(media_id, title)));

      // Get favicon for tab.
      favicon::FaviconDriver* favicon_driver =
          favicon::ContentFaviconDriver::FromWebContents(contents);
      if (!favicon_driver)
        continue;

      gfx::Image favicon = favicon_driver->GetFavicon();
      if (favicon.IsEmpty())
        continue;

      // Only new or changed favicon need update.
      new_favicon_hashes[media_id] = GetImageHash(favicon);
      if (!favicon_hashes_.count(media_id) ||
          (favicon_hashes_[media_id] != new_favicon_hashes[media_id])) {
        gfx::ImageSkia image = favicon.AsImageSkia();
        image.MakeThreadSafe();
        favicon_pairs.push_back(std::make_pair(media_id, image));
      }
    }
  }
  favicon_hashes_ = new_favicon_hashes;

  // Sort tab sources by time. Most recent one first. Then update sources list.
  for (auto it = tab_map.rbegin(); it != tab_map.rend(); ++it) {
    sources.push_back(it->second);
  }
  UpdateSourcesList(sources);

  for (const auto& it : favicon_pairs) {
    // Create a thumbail in a different thread and update the thumbnail in
    // current thread.
    base::PostTaskAndReplyWithResult(
        thumbnail_task_runner_.get(), FROM_HERE,
        base::Bind(&CreateEnlargedFaviconImage, thumbnail_size_, it.second),
        base::Bind(&TabDesktopMediaList::UpdateSourceThumbnail,
                   weak_factory_.GetWeakPtr(), it.first));
  }

  // ScheduleNextRefresh() needs to be called after all calls for
  // UpdateSourceThumbnail() have done. Therefore, a DoNothing task is posted
  // to the same sequenced task runner that CreateEnlargedFaviconImag()
  // is posted.
  thumbnail_task_runner_.get()->PostTaskAndReply(
      FROM_HERE, base::Bind(&base::DoNothing),
      base::Bind(&TabDesktopMediaList::ScheduleNextRefresh,
                 weak_factory_.GetWeakPtr()));
}
