// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/pdf_browsertest_base.h"

#include <algorithm>
#include <vector>

#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/screen.h"

#if defined(OS_LINUX)
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#endif

#if defined(OS_CHROMEOS)
#include "ui/compositor/compositor_switches.h"
#endif

namespace {

// Include things like browser frame and scrollbar and make sure we're bigger
// than the test pdf document.
const int kBrowserWidth = 1000;
const int kBrowserHeight = 600;

}  // namespace

PDFBrowserTest::PDFBrowserTest()
    : snapshot_different_(true),
      next_dummy_search_value_(0),
      load_stop_notification_count_(0) {
  base::FilePath src_dir;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
  pdf_test_server_.ServeFilesFromDirectory(src_dir.AppendASCII(
      "chrome/test/data/pdf_private"));
}

PDFBrowserTest::~PDFBrowserTest() {
}

void PDFBrowserTest::Load() {
  // Make sure to set the window size before rendering, as otherwise rendering
  // to a smaller window and then expanding leads to slight anti-aliasing
  // differences of the text and the pixel comparison fails.
  gfx::Rect bounds(gfx::Rect(0, 0, kBrowserWidth, kBrowserHeight));
  gfx::Rect screen_bounds =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().bounds();
  ASSERT_GT(screen_bounds.width(), kBrowserWidth);
  ASSERT_GT(screen_bounds.height(), kBrowserHeight);
  browser()->window()->SetBounds(bounds);

  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(FILE_PATH_LITERAL("pdf_private")),
      base::FilePath(FILE_PATH_LITERAL("pdf_browsertest.pdf"))));
  ui_test_utils::NavigateToURL(browser(), url);
}

void PDFBrowserTest::WaitForResponse() {
  // Even if the plugin has loaded the data or scrolled, because of how
  // pepper painting works, we might not have the data.  One way to force this
  // to be flushed is to do a find operation, since on this two-page test
  // document, it'll wait for us to flush the renderer message loop twice and
  // also the browser's once, at which point we're guaranteed to have updated
  // the backingstore.  Hacky, but it works.
  // Note that we need to change the text each time, because if we don't the
  // renderer code will think the second message is to go to next result, but
  // there are none so the plugin will assert.

  base::string16 query = base::UTF8ToUTF16(
      std::string("xyzxyz" + base::IntToString(next_dummy_search_value_++)));
  ASSERT_EQ(0, ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      query, true, false, NULL, NULL));
}

bool PDFBrowserTest::VerifySnapshot(const std::string& expected_filename) {
  snapshot_different_ = true;
  expected_filename_ = expected_filename;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(web_contents);

  content::RenderWidgetHost* rwh = web_contents->GetRenderViewHost();
  rwh->CopyFromBackingStore(
      gfx::Rect(),
      gfx::Size(),
      base::Bind(&PDFBrowserTest::CopyFromBackingStoreCallback, this),
      kN32_SkColorType);

  content::RunMessageLoop();

  if (snapshot_different_) {
    LOG(INFO) << "Rendering didn't match, see result "
              << snapshot_filename_.value();
  }
  return !snapshot_different_;
}

void PDFBrowserTest::CopyFromBackingStoreCallback(bool success,
                                                  const SkBitmap& bitmap) {
  base::MessageLoopForUI::current()->Quit();
  ASSERT_EQ(success, true);
  base::FilePath reference = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("pdf_private")),
      base::FilePath().AppendASCII(expected_filename_));
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(reference, &info));
  int size = static_cast<size_t>(info.size);
  scoped_ptr<char[]> data(new char[size]);
  ASSERT_EQ(size, base::ReadFile(reference, data.get(), size));

  int w, h;
  std::vector<unsigned char> decoded;
  ASSERT_TRUE(gfx::PNGCodec::Decode(
      reinterpret_cast<unsigned char*>(data.get()), size,
      gfx::PNGCodec::FORMAT_BGRA, &decoded, &w, &h));
  int32* ref_pixels = reinterpret_cast<int32*>(&decoded[0]);

  int32* pixels = static_cast<int32*>(bitmap.getPixels());

  // Get the background color, and use it to figure out the x-offsets in
  // each image.  The reason is that depending on the theme in the OS, the
  // same browser width can lead to slightly different plugin sizes, so the
  // pdf content will start at different x offsets.
  // Also note that the images we saved are cut off before the scrollbar, as
  // that'll change depending on the theme, and also cut off vertically so
  // that the ui controls don't show up, as those fade-in and so the timing
  // will affect their transparency.
  int32 bg_color = ref_pixels[0];
  int ref_x_offset, snapshot_x_offset;
  for (ref_x_offset = 0; ref_x_offset < w; ++ref_x_offset) {
    if (ref_pixels[ref_x_offset] != bg_color)
      break;
  }

  for (snapshot_x_offset = 0; snapshot_x_offset < bitmap.width();
       ++snapshot_x_offset) {
    if (pixels[snapshot_x_offset] != bg_color)
      break;
  }

  int x_max = std::min(w - ref_x_offset, bitmap.width() - snapshot_x_offset);
  int y_max = std::min(h, bitmap.height());
  int stride = bitmap.rowBytes();
  snapshot_different_ = false;
  for (int y = 0; y < y_max && !snapshot_different_; ++y) {
    for (int x = 0; x < x_max && !snapshot_different_; ++x) {
      if (pixels[y * stride / sizeof(int32) + x + snapshot_x_offset] !=
          ref_pixels[y * w + x + ref_x_offset])
        snapshot_different_ = true;
    }
  }

  if (snapshot_different_) {
    std::vector<unsigned char> png_data;
    gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_data);
    if (base::CreateTemporaryFile(&snapshot_filename_)) {
      base::WriteFile(snapshot_filename_,
                      reinterpret_cast<char*>(&png_data[0]), png_data.size());
    }
  }
}

void PDFBrowserTest::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_LOAD_STOP, type);
  load_stop_notification_count_++;
}

void PDFBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
#if defined(OS_LINUX)
  // Calling RenderWidgetHost::CopyFromBackingStore() with the GPU enabled
  // fails on Linux.
  command_line->AppendSwitch(switches::kDisableGpu);
#endif

#if defined(OS_CHROMEOS)
  // Also need on CrOS in addition to disabling the GPU above.
  command_line->AppendSwitch(switches::kUIDisableThreadedCompositing);
#endif
}
