// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/hash.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/screen.h"

using content::NavigationController;
using content::WebContents;

// Note: All tests in here require the internal PDF plugin, so they're disabled
// in non-official builds. We still compile them though, to prevent bitrot.

namespace {

// Include things like browser frame and scrollbar and make sure we're bigger
// than the test pdf document.
static const int kBrowserWidth = 1000;
static const int kBrowserHeight = 600;

class PDFBrowserTest : public InProcessBrowserTest,
                       public testing::WithParamInterface<int>,
                       public content::NotificationObserver {
 public:
  PDFBrowserTest()
      : snapshot_different_(true),
        next_dummy_search_value_(0),
        load_stop_notification_count_(0) {
    base::FilePath src_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    pdf_test_server_.ServeFilesFromDirectory(src_dir.AppendASCII(
        "chrome/test/data/pdf_private"));
  }

 protected:
  // Use our own TestServer so that we can serve files from the pdf directory.
  net::test_server::EmbeddedTestServer* pdf_test_server() {
    return &pdf_test_server_;
  }

  int load_stop_notification_count() const {
    return load_stop_notification_count_;
  }

  void Load() {
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

  bool VerifySnapshot(const std::string& expected_filename) {
    snapshot_different_ = true;
    expected_filename_ = expected_filename;
    WebContents* web_contents =
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
      LOG(INFO) << "Rendering didn't match, see result " <<
          snapshot_filename_.value();
    }
    return !snapshot_different_;
  }

  void WaitForResponse() {
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

 private:
  void CopyFromBackingStoreCallback(bool success, const SkBitmap& bitmap) {
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

    int x_max = std::min(
        w - ref_x_offset, bitmap.width() - snapshot_x_offset);
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

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == content::NOTIFICATION_LOAD_STOP) {
      load_stop_notification_count_++;
    }
  }

  // InProcessBrowserTest
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
#if defined(OS_LINUX)
    // Calling RenderWidgetHost::CopyFromBackingStore() with the GPU enabled
    // fails on Linux.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  // True if the snapshot differed from the expected value.
  bool snapshot_different_;
  // Internal variable used to synchronize to the renderer.
  int next_dummy_search_value_;
  // The filename of the bitmap to compare the snapshot to.
  std::string expected_filename_;
  // If the snapshot is different, holds the location where it's saved.
  base::FilePath snapshot_filename_;
  // How many times we've seen chrome::LOAD_STOP.
  int load_stop_notification_count_;

  net::test_server::EmbeddedTestServer pdf_test_server_;
};


// Tests basic PDF rendering.  This can be broken depending on bad merges with
// the vendor, so it's important that we have basic sanity checking.
#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX)
#define MAYBE_Basic Basic
#else
#define MAYBE_Basic DISABLED_Basic
#endif
IN_PROC_BROWSER_TEST_F(PDFBrowserTest, MAYBE_Basic) {
  ASSERT_NO_FATAL_FAILURE(Load());
  ASSERT_NO_FATAL_FAILURE(WaitForResponse());
  // OS X uses CoreText, and FreeType renders slightly different on Linux and
  // Win.
#if defined(OS_MACOSX)
  // The bots render differently than locally, see http://crbug.com/142531.
  ASSERT_TRUE(VerifySnapshot("pdf_browsertest_mac.png") ||
              VerifySnapshot("pdf_browsertest_mac2.png"));
#elif defined(OS_LINUX)
  ASSERT_TRUE(VerifySnapshot("pdf_browsertest_linux.png"));
#else
  ASSERT_TRUE(VerifySnapshot("pdf_browsertest.png"));
#endif
}

#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_WIN) || defined(OS_LINUX))
#define MAYBE_Scroll Scroll
#else
// TODO(thestig): http://crbug.com/79837, http://crbug.com/332778
#define MAYBE_Scroll DISABLED_Scroll
#endif
// Tests that scrolling works.
IN_PROC_BROWSER_TEST_F(PDFBrowserTest, MAYBE_Scroll) {
  ASSERT_NO_FATAL_FAILURE(Load());

  // We use wheel mouse event since that's the only one we can easily push to
  // the renderer.  There's no way to push a cross-platform keyboard event at
  // the moment.
  blink::WebMouseWheelEvent wheel_event;
  wheel_event.type = blink::WebInputEvent::MouseWheel;
  wheel_event.deltaY = -200;
  wheel_event.wheelTicksY = -2;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetRenderViewHost()->ForwardWheelEvent(wheel_event);
  ASSERT_NO_FATAL_FAILURE(WaitForResponse());

  int y_offset = 0;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(plugin.pageYOffset())",
      &y_offset));
  ASSERT_GT(y_offset, 0);
}

#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_WIN) || defined(OS_LINUX))
#define MAYBE_FindAndCopy FindAndCopy
#else
// TODO(thestig): http://crbug.com/79837, http://crbug.com/329912
#define MAYBE_FindAndCopy DISABLED_FindAndCopy
#endif
IN_PROC_BROWSER_TEST_F(PDFBrowserTest, MAYBE_FindAndCopy) {
  ASSERT_NO_FATAL_FAILURE(Load());
  // Verifies that find in page works.
  ASSERT_EQ(3, ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::UTF8ToUTF16("adipiscing"),
      true, false, NULL, NULL));

  // Verify that copying selected text works.
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  // Reset the clipboard first.
  clipboard->Clear(ui::CLIPBOARD_TYPE_COPY_PASTE);

  browser()->tab_strip_model()->GetActiveWebContents()->Copy();
  ASSERT_NO_FATAL_FAILURE(WaitForResponse());

  std::string text;
  clipboard->ReadAsciiText(ui::CLIPBOARD_TYPE_COPY_PASTE, &text);
  ASSERT_EQ("adipiscing", text);
}

const int kLoadingNumberOfParts = 10;

// Tests that loading async pdfs works correctly (i.e. document fully loads).
// This also loads all documents that used to crash, to ensure we don't have
// regressions.
// If it flakes, reopen http://crbug.com/74548.
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_Loading Loading
#else
#define MAYBE_Loading DISABLED_Loading
#endif
IN_PROC_BROWSER_TEST_P(PDFBrowserTest, MAYBE_Loading) {
  ASSERT_TRUE(pdf_test_server()->InitializeAndWaitUntilReady());

  NavigationController* controller =
      &(browser()->tab_strip_model()->GetActiveWebContents()->GetController());
  content::NotificationRegistrar registrar;
  registrar.Add(this,
                content::NOTIFICATION_LOAD_STOP,
                content::Source<NavigationController>(controller));
  std::string base_url = std::string("/");

  base::FileEnumerator file_enumerator(
      ui_test_utils::GetTestFilePath(
          base::FilePath(FILE_PATH_LITERAL("pdf_private")), base::FilePath()),
      false,
      base::FileEnumerator::FILES,
      FILE_PATH_LITERAL("*.pdf"));
  for (base::FilePath file_path = file_enumerator.Next();
       !file_path.empty();
       file_path = file_enumerator.Next()) {
    std::string filename = file_path.BaseName().MaybeAsASCII();
    ASSERT_FALSE(filename.empty());

#if defined(OS_POSIX)
    if (filename == "sample.pdf")
      continue;  // Crashes on Mac and Linux.  http://crbug.com/63549
#endif

    // Split the test into smaller sub-tests. Each one only loads
    // every k-th file.
    if (static_cast<int>(base::Hash(filename) % kLoadingNumberOfParts) !=
        GetParam()) {
      continue;
    }

    LOG(WARNING) << "PDFBrowserTest.Loading: " << filename;

    GURL url = pdf_test_server()->GetURL(base_url + filename);
    ui_test_utils::NavigateToURL(browser(), url);

    while (true) {
      int last_count = load_stop_notification_count();
      // We might get extraneous chrome::LOAD_STOP notifications when
      // doing async loading.  This happens when the first loader is cancelled
      // and before creating a byte-range request loader.
      bool complete = false;
      ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
          browser()->tab_strip_model()->GetActiveWebContents(),
          "window.domAutomationController.send(plugin.documentLoadComplete())",
          &complete));
      if (complete)
        break;

      // Check if the LOAD_STOP notification could have come while we run a
      // nested message loop for the JS call.
      if (last_count != load_stop_notification_count())
        continue;
      content::WaitForLoadStop(
          browser()->tab_strip_model()->GetActiveWebContents());
    }
  }
}

INSTANTIATE_TEST_CASE_P(PDFTestFiles,
                        PDFBrowserTest,
                        testing::Range(0, kLoadingNumberOfParts));

#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_WIN) || defined(OS_LINUX))
#define MAYBE_Action Action
#else
// http://crbug.com/315160
#define MAYBE_Action DISABLED_Action
#endif
IN_PROC_BROWSER_TEST_F(PDFBrowserTest, MAYBE_Action) {
  ASSERT_NO_FATAL_FAILURE(Load());

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementsByName('plugin')[0].fitToHeight();"));

  std::string zoom1, zoom2;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "    document.getElementsByName('plugin')[0].getZoomLevel().toString())",
      &zoom1));

  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "document.getElementsByName('plugin')[0].fitToWidth();"));

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "    document.getElementsByName('plugin')[0].getZoomLevel().toString())",
      &zoom2));
  ASSERT_NE(zoom1, zoom2);
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX)
#define MAYBE_OnLoadAndReload OnLoadAndReload
#else
// Flaky as per http://crbug.com/74549.
#define MAYBE_OnLoadAndReload DISABLED_OnLoadAndReload
#endif
IN_PROC_BROWSER_TEST_F(PDFBrowserTest, MAYBE_OnLoadAndReload) {
  ASSERT_TRUE(pdf_test_server()->InitializeAndWaitUntilReady());

  GURL url = pdf_test_server()->GetURL("/onload_reload.html");
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &contents->GetController()));
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "reloadPDF();"));
  observer.Wait();

  ASSERT_EQ("success", contents->GetURL().query());
}

}  // namespace
