// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/metro_pin_tab_helper_win.h"

#include <set>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/metro.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/favicon/favicon_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(MetroPinTabHelper);

namespace {

// Histogram name for site-specific tile pinning metrics.
const char kMetroPinMetric[] = "Metro.SecondaryTilePin";

// Generate an ID for the tile based on |url_str|. The ID is simply a hash of
// the URL.
string16 GenerateTileId(const string16& url_str) {
  uint8 hash[crypto::kSHA256Length];
  crypto::SHA256HashString(UTF16ToUTF8(url_str), hash, sizeof(hash));
  std::string hash_str = base::HexEncode(hash, sizeof(hash));
  return UTF8ToUTF16(hash_str);
}

// Get the path of the directory to store the tile logos in.
FilePath GetTileImagesDir() {
  FilePath tile_images_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &tile_images_dir))
    return FilePath();

  tile_images_dir = tile_images_dir.Append(L"TileImages");
  if (!file_util::DirectoryExists(tile_images_dir) &&
      !file_util::CreateDirectory(tile_images_dir))
    return FilePath();

  return tile_images_dir;
}

// For the given |image| and |tile_id|, try to create a site specific logo in
// |logo_dir|. The path of any created logo is returned in |logo_path|. Return
// value indicates whether a site specific logo was created.
bool CreateSiteSpecificLogo(const gfx::ImageSkia& image,
                            const string16& tile_id,
                            const FilePath& logo_dir,
                            FilePath* logo_path) {
  const int kLogoWidth = 120;
  const int kLogoHeight = 120;
  const int kBoxWidth = 40;
  const int kBoxHeight = 40;
  const int kCaptionHeight = 20;
  const double kBoxFade = 0.75;

  if (image.isNull())
    return false;

  // Fill the tile logo with the dominant color of the favicon bitmap.
  SkColor dominant_color = color_utils::CalculateKMeanColorOfBitmap(
      image.GetRepresentation(ui::SCALE_FACTOR_100P).sk_bitmap());
  SkPaint paint;
  paint.setColor(dominant_color);
  gfx::Canvas canvas(gfx::Size(kLogoWidth, kLogoHeight), ui::SCALE_FACTOR_100P,
                     true);
  canvas.DrawRect(gfx::Rect(0, 0, kLogoWidth, kLogoHeight), paint);

  // Now paint a faded square for the favicon to go in.
  color_utils::HSL shift = {-1, -1, kBoxFade};
  paint.setColor(color_utils::HSLShift(dominant_color, shift));
  int box_left = (kLogoWidth - kBoxWidth) / 2;
  int box_top = (kLogoHeight - kCaptionHeight - kBoxHeight) / 2;
  canvas.DrawRect(gfx::Rect(box_left, box_top, kBoxWidth, kBoxHeight), paint);

  // Now paint the favicon into the tile, leaving some room at the bottom for
  // the caption.
  int left = (kLogoWidth - image.width()) / 2;
  int top = (kLogoHeight - kCaptionHeight - image.height()) / 2;
  canvas.DrawImageInt(image, left, top);

  SkBitmap logo_bitmap = canvas.ExtractImageRep().sk_bitmap();
  std::vector<unsigned char> logo_png;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(logo_bitmap, true, &logo_png))
    return false;

  *logo_path = logo_dir.Append(tile_id).ReplaceExtension(L".png");
  return file_util::WriteFile(*logo_path,
                              reinterpret_cast<char*>(&logo_png[0]),
                              logo_png.size()) > 0;
}

// Get the path to the backup logo. If the backup logo already exists in
// |logo_dir|, it will be used, otherwise it will be copied out of the install
// folder. (The version in the install folder is not used as it may disappear
// after an upgrade, causing tiles to lose their images if Windows rebuilds
// its tile image cache.)
// The path to the logo is returned in |logo_path|, with the return value
// indicating success.
bool GetPathToBackupLogo(const FilePath& logo_dir,
                         FilePath* logo_path) {
  const wchar_t kDefaultLogoFileName[] = L"SecondaryTile.png";
  *logo_path = logo_dir.Append(kDefaultLogoFileName);
  if (file_util::PathExists(*logo_path))
    return true;

  FilePath default_logo_path;
  if (!PathService::Get(base::DIR_MODULE, &default_logo_path))
    return false;

  default_logo_path = default_logo_path.Append(kDefaultLogoFileName);
  return file_util::CopyFile(default_logo_path, *logo_path);
}

// UMA reporting callback for site-specific secondary tile creation.
void PinPageReportUmaCallback(
    base::win::MetroSecondaryTilePinUmaResult result) {
  UMA_HISTOGRAM_ENUMERATION(kMetroPinMetric,
                            result,
                            base::win::METRO_PIN_STATE_LIMIT);
}

// The PinPageTaskRunner class performs the necessary FILE thread actions to
// pin a page, such as generating or copying the tile image file. When it
// has performed these actions it will send the tile creation request to the
// metro driver.
class PinPageTaskRunner : public base::RefCountedThreadSafe<PinPageTaskRunner> {
 public:
  // Creates a task runner for the pinning operation with the given details.
  // |favicon| can be a null image (i.e. favicon.isNull() can be true), in
  // which case the backup tile image will be used.
  PinPageTaskRunner(const string16& title,
                    const string16& url,
                    const gfx::ImageSkia& favicon);

  void Run();
  void RunOnFileThread();

 private:
  ~PinPageTaskRunner() {}

  // Details of the page being pinned.
  const string16 title_;
  const string16 url_;
  gfx::ImageSkia favicon_;

  friend class base::RefCountedThreadSafe<PinPageTaskRunner>;
  DISALLOW_COPY_AND_ASSIGN(PinPageTaskRunner);
};

PinPageTaskRunner::PinPageTaskRunner(const string16& title,
                                     const string16& url,
                                     const gfx::ImageSkia& favicon)
    : title_(title),
      url_(url),
      favicon_(favicon) {}

void PinPageTaskRunner::Run() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&PinPageTaskRunner::RunOnFileThread, this));
}

void PinPageTaskRunner::RunOnFileThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  string16 tile_id = GenerateTileId(url_);
  FilePath logo_dir = GetTileImagesDir();
  if (logo_dir.empty()) {
    LOG(ERROR) << "Could not create directory to store tile image.";
    return;
  }

  FilePath logo_path;
  if (!CreateSiteSpecificLogo(favicon_, tile_id, logo_dir, &logo_path) &&
      !GetPathToBackupLogo(logo_dir, &logo_path)) {
    LOG(ERROR) << "Count not get path to logo tile.";
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kMetroPinMetric,
                            base::win::METRO_PIN_LOGO_READY,
                            base::win::METRO_PIN_STATE_LIMIT);

  HMODULE metro_module = base::win::GetMetroModule();
  if (!metro_module)
    return;

  base::win::MetroPinToStartScreen metro_pin_to_start_screen =
      reinterpret_cast<base::win::MetroPinToStartScreen>(
          ::GetProcAddress(metro_module, "MetroPinToStartScreen"));
  if (!metro_pin_to_start_screen) {
    NOTREACHED();
    return;
  }

  metro_pin_to_start_screen(tile_id,
                            title_,
                            url_,
                            logo_path,
                            base::Bind(&PinPageReportUmaCallback));
}

}  // namespace

class MetroPinTabHelper::FaviconChooser {
 public:
  FaviconChooser(MetroPinTabHelper* helper,
                 const string16& title,
                 const string16& url,
                 const gfx::ImageSkia& history_image);

  ~FaviconChooser() {}

  // Pin the page on the FILE thread using the current |best_candidate_| and
  // delete the FaviconChooser.
  void UseChosenCandidate();

  // Update the |best_candidate_| with the newly downloaded favicons provided.
  void UpdateCandidate(int id,
                       const GURL& image_url,
                       bool errored,
                       int requested_size,
                       const std::vector<SkBitmap>& bitmaps);

  void AddPendingRequest(int request_id);

 private:
  // The tab helper that this chooser is operating for.
  MetroPinTabHelper* helper_;

  // Title and URL of the page being pinned.
  const string16 title_;
  const string16 url_;

  // The best candidate we have so far for the current pin operation.
  gfx::ImageSkia best_candidate_;

  // Outstanding favicon download requests.
  std::set<int> in_progress_requests_;

  DISALLOW_COPY_AND_ASSIGN(FaviconChooser);
};

MetroPinTabHelper::FaviconChooser::FaviconChooser(
    MetroPinTabHelper* helper,
    const string16& title,
    const string16& url,
    const gfx::ImageSkia& history_image)
        : helper_(helper),
          title_(title),
          url_(url),
          best_candidate_(history_image) {}

void MetroPinTabHelper::FaviconChooser::UseChosenCandidate() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scoped_refptr<PinPageTaskRunner> runner(
      new PinPageTaskRunner(title_, url_, best_candidate_));
  runner->Run();
  helper_->FaviconDownloaderFinished();
}

void MetroPinTabHelper::FaviconChooser::UpdateCandidate(
    int id,
    const GURL& image_url,
    bool errored,
    int requested_size,
    const std::vector<SkBitmap>& bitmaps) {
  const int kMaxIconSize = 32;

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::set<int>::iterator iter = in_progress_requests_.find(id);
  // Check that this request is one of ours.
  if (iter == in_progress_requests_.end())
    return;

  in_progress_requests_.erase(iter);

  // Process the bitmaps, keeping the one that is best so far.
   if (!errored) {
    for (std::vector<SkBitmap>::const_iterator iter = bitmaps.begin();
         iter != bitmaps.end();
         ++iter) {

      // If the new bitmap is too big, ignore it.
      if (iter->height() > kMaxIconSize || iter->width() > kMaxIconSize)
        continue;

      // If we don't have a best candidate yet, this is better so just grab it.
      if (best_candidate_.isNull()) {
        best_candidate_ = *gfx::ImageSkia(*iter).DeepCopy().get();
        continue;
      }

      // If it is smaller than our best one so far, ignore it.
      if (iter->height() <= best_candidate_.height() ||
          iter->width() <= best_candidate_.width()) {
        continue;
      }

      // Othewise it is our new best candidate.
      best_candidate_ = *gfx::ImageSkia(*iter).DeepCopy().get();
    }
  }

  // If there are no more outstanding requests, pin the page on the FILE thread.
  // Once this happens this downloader has done its job, so delete it.
  if (in_progress_requests_.empty())
    UseChosenCandidate();
}

void MetroPinTabHelper::FaviconChooser::AddPendingRequest(int request_id) {
  in_progress_requests_.insert(request_id);
}

MetroPinTabHelper::MetroPinTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

MetroPinTabHelper::~MetroPinTabHelper() {}

bool MetroPinTabHelper::IsPinned() const {
  HMODULE metro_module = base::win::GetMetroModule();
  if (!metro_module)
    return false;

  typedef BOOL (*MetroIsPinnedToStartScreen)(const string16&);
  MetroIsPinnedToStartScreen metro_is_pinned_to_start_screen =
      reinterpret_cast<MetroIsPinnedToStartScreen>(
          ::GetProcAddress(metro_module, "MetroIsPinnedToStartScreen"));
  if (!metro_is_pinned_to_start_screen) {
    NOTREACHED();
    return false;
  }

  GURL url = web_contents()->GetURL();
  string16 tile_id = GenerateTileId(UTF8ToUTF16(url.spec()));
  return metro_is_pinned_to_start_screen(tile_id) != 0;
}

void MetroPinTabHelper::TogglePinnedToStartScreen() {
  if (IsPinned()) {
    UMA_HISTOGRAM_ENUMERATION(kMetroPinMetric,
                              base::win::METRO_UNPIN_INITIATED,
                              base::win::METRO_PIN_STATE_LIMIT);
    UnPinPageFromStartScreen();
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kMetroPinMetric,
                            base::win::METRO_PIN_INITIATED,
                            base::win::METRO_PIN_STATE_LIMIT);
  GURL url = web_contents()->GetURL();
  string16 url_str = UTF8ToUTF16(url.spec());
  string16 title = web_contents()->GetTitle();
  // TODO(oshima): Use scoped_ptr::Pass to pass it to other thread.
  gfx::ImageSkia favicon;
  FaviconTabHelper* favicon_tab_helper = FaviconTabHelper::FromWebContents(
      web_contents());
  if (favicon_tab_helper->FaviconIsValid())
    favicon = *favicon_tab_helper->GetFavicon().AsImageSkia().DeepCopy().get();

  favicon_chooser_.reset(new FaviconChooser(this, title, url_str, favicon));

  if (favicon_url_candidates_.empty()) {
    favicon_chooser_->UseChosenCandidate();
    return;
  }

  // Request all the candidates.
  int image_size = 0; // Request the full sized image.
  for (std::vector<content::FaviconURL>::const_iterator iter =
           favicon_url_candidates_.begin();
       iter != favicon_url_candidates_.end();
       ++iter) {
    favicon_chooser_->AddPendingRequest(
        web_contents()->DownloadFavicon(iter->icon_url, image_size,
            base::Bind(&MetroPinTabHelper::DidDownloadFavicon,
                       base::Unretained(this))));
  }

}

void MetroPinTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& /*details*/,
    const content::FrameNavigateParams& /*params*/) {
  // Cancel any outstanding pin operations once the user navigates away from
  // the page.
  if (favicon_chooser_.get())
    favicon_chooser_.reset();
  // Any candidate favicons we have are now out of date so clear them.
  favicon_url_candidates_.clear();
}

void MetroPinTabHelper::DidUpdateFaviconURL(
    int32 page_id,
    const std::vector<content::FaviconURL>& candidates) {
  favicon_url_candidates_ = candidates;
}

void MetroPinTabHelper::DidDownloadFavicon(
    int id,
    const GURL& image_url,
    bool errored,
    int requested_size,
    const std::vector<SkBitmap>& bitmaps) {
  if (favicon_chooser_.get()) {
    favicon_chooser_->UpdateCandidate(id, image_url, errored,
                                      requested_size, bitmaps);
  }
}

void MetroPinTabHelper::UnPinPageFromStartScreen() {
  HMODULE metro_module = base::win::GetMetroModule();
  if (!metro_module)
    return;

  base::win::MetroUnPinFromStartScreen metro_un_pin_from_start_screen =
      reinterpret_cast<base::win::MetroUnPinFromStartScreen>(
          ::GetProcAddress(metro_module, "MetroUnPinFromStartScreen"));
  if (!metro_un_pin_from_start_screen) {
    NOTREACHED();
    return;
  }

  GURL url = web_contents()->GetURL();
  string16 tile_id = GenerateTileId(UTF8ToUTF16(url.spec()));
  metro_un_pin_from_start_screen(tile_id,
                                 base::Bind(&PinPageReportUmaCallback));
}

void MetroPinTabHelper::FaviconDownloaderFinished() {
  favicon_chooser_.reset();
}
