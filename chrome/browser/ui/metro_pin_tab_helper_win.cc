// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/metro_pin_tab_helper_win.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/metro.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(MetroPinTabHelper)

namespace {

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
  const int kColorMeanDarknessLimit = 100;
  const int kColorMeanLightnessLimit = 650;

  if (image.isNull())
    return false;

  *logo_path = logo_dir.Append(tile_id).ReplaceExtension(L".png");

  // Use a canvas to paint the tile logo.
  gfx::Canvas canvas(gfx::Size(kLogoWidth, kLogoHeight), ui::SCALE_FACTOR_100P,
                     true);

  // Fill the tile logo with the average color from bitmap. To do this we need
  // to work out the 'average color' which is calculated using PNG encoded data
  // of the bitmap.
  SkPaint paint;
  std::vector<unsigned char> icon_png;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(*image.bitmap(), true, &icon_png))
    return false;

  scoped_refptr<base::RefCountedStaticMemory> icon_mem(
      new base::RefCountedStaticMemory(&icon_png.front(), icon_png.size()));
  color_utils::GridSampler sampler;
  SkColor mean_color = color_utils::CalculateKMeanColorOfPNG(
      icon_mem, kColorMeanDarknessLimit, kColorMeanLightnessLimit, sampler);
  paint.setColor(mean_color);
  canvas.DrawRect(gfx::Rect(0, 0, kLogoWidth, kLogoHeight), paint);

  // Now paint a faded square for the favicon to go in.
  color_utils::HSL shift = {-1, -1, kBoxFade};
  paint.setColor(color_utils::HSLShift(mean_color, shift));
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

}  // namespace

class MetroPinTabHelper::TaskRunner
    : public base::RefCountedThreadSafe<TaskRunner> {
 public:
  TaskRunner() {}

  void PinPageToStartScreen(const string16& title,
                            const string16& url,
                            const gfx::ImageSkia& image);

 private:
  ~TaskRunner() {}

  friend class base::RefCountedThreadSafe<TaskRunner>;
  DISALLOW_COPY_AND_ASSIGN(TaskRunner);
};

void MetroPinTabHelper::TaskRunner::PinPageToStartScreen(
    const string16& title,
    const string16& url,
    const gfx::ImageSkia& image) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  string16 tile_id = GenerateTileId(url);
  FilePath logo_dir = GetTileImagesDir();
  if (logo_dir.empty()) {
    LOG(ERROR) << "Could not create directory to store tile image.";
    return;
  }

  FilePath logo_path;
  if (!CreateSiteSpecificLogo(image, tile_id, logo_dir, &logo_path) &&
      !GetPathToBackupLogo(logo_dir, &logo_path)) {
    LOG(ERROR) << "Count not get path to logo tile.";
    return;
  }

  HMODULE metro_module = base::win::GetMetroModule();
  if (!metro_module)
    return;

  typedef void (*MetroPinToStartScreen)(const string16&, const string16&,
      const string16&, const FilePath&);
  MetroPinToStartScreen metro_pin_to_start_screen =
      reinterpret_cast<MetroPinToStartScreen>(
          ::GetProcAddress(metro_module, "MetroPinToStartScreen"));
  if (!metro_pin_to_start_screen) {
    NOTREACHED();
    return;
  }

  VLOG(1) << __FUNCTION__ << " calling pin with title: " << title
          << " and url: " << url;
  metro_pin_to_start_screen(tile_id, title, url, logo_path);
}

MetroPinTabHelper::MetroPinTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      is_pinned_(false),
      task_runner_(new TaskRunner) {}

MetroPinTabHelper::~MetroPinTabHelper() {}

void MetroPinTabHelper::TogglePinnedToStartScreen() {
  UpdatePinnedStateForCurrentURL();
  bool was_pinned = is_pinned_;

  // TODO(benwells): This will update the state incorrectly if the user
  // cancels. To fix this some sort of callback needs to be introduced as
  // the pinning happens on another thread.
  is_pinned_ = !is_pinned_;

  if (was_pinned) {
    UnPinPageFromStartScreen();
    return;
  }

  // TODO(benwells): Handle downloading a larger favicon if there is one.
  GURL url = web_contents()->GetURL();
  string16 url_str = UTF8ToUTF16(url.spec());
  string16 title = web_contents()->GetTitle();
  TabContents* tab_contents = TabContents::FromWebContents(web_contents());
  DCHECK(tab_contents);
  FaviconTabHelper* favicon_tab_helper = FaviconTabHelper::FromWebContents(
      tab_contents->web_contents());
  if (favicon_tab_helper->FaviconIsValid()) {
    gfx::Image favicon = favicon_tab_helper->GetFavicon();
    gfx::ImageSkia favicon_skia = favicon.AsImageSkia().DeepCopy();
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&TaskRunner::PinPageToStartScreen,
                   task_runner_,
                   title,
                   url_str,
                   favicon_skia));
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&TaskRunner::PinPageToStartScreen,
                 task_runner_,
                 title,
                 url_str,
                 gfx::ImageSkia()));
}

void MetroPinTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& /*details*/,
    const content::FrameNavigateParams& /*params*/) {
  UpdatePinnedStateForCurrentURL();
}

void MetroPinTabHelper::UpdatePinnedStateForCurrentURL() {
  HMODULE metro_module = base::win::GetMetroModule();
  if (!metro_module)
    return;

  typedef BOOL (*MetroIsPinnedToStartScreen)(const string16&);
  MetroIsPinnedToStartScreen metro_is_pinned_to_start_screen =
      reinterpret_cast<MetroIsPinnedToStartScreen>(
          ::GetProcAddress(metro_module, "MetroIsPinnedToStartScreen"));
  if (!metro_is_pinned_to_start_screen) {
    NOTREACHED();
    return;
  }

  GURL url = web_contents()->GetURL();
  string16 tile_id = GenerateTileId(UTF8ToUTF16(url.spec()));
  is_pinned_ = metro_is_pinned_to_start_screen(tile_id) != 0;
  VLOG(1) << __FUNCTION__ << " with url " << UTF8ToUTF16(url.spec())
          << " result: " << is_pinned_;
}

void MetroPinTabHelper::UnPinPageFromStartScreen() {
  HMODULE metro_module = base::win::GetMetroModule();
  if (!metro_module)
    return;

  typedef void (*MetroUnPinFromStartScreen)(const string16&);
  MetroUnPinFromStartScreen metro_un_pin_from_start_screen =
      reinterpret_cast<MetroUnPinFromStartScreen>(
          ::GetProcAddress(metro_module, "MetroUnPinFromStartScreen"));
  if (!metro_un_pin_from_start_screen) {
    NOTREACHED();
    return;
  }

  GURL url = web_contents()->GetURL();
  VLOG(1) << __FUNCTION__ << " calling unpin with url: "
          << UTF8ToUTF16(url.spec());
  string16 tile_id = GenerateTileId(UTF8ToUTF16(url.spec()));
  metro_un_pin_from_start_screen(tile_id);
}
