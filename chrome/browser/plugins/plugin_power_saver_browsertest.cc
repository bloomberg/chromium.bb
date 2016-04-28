// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ui/zoom/zoom_controller.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/ppapi_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/switches.h"

namespace {

// Use fixed browser dimensions for pixel tests.
const int kBrowserWidth = 600;
const int kBrowserHeight = 700;

// Compare only a portion of the snapshots, as different platforms will
// capture different sized snapshots (due to differences in browser chrome).
const int kComparisonWidth = 500;
const int kComparisonHeight = 600;

// Different platforms have slightly different pixel output, due to different
// graphics implementations. Slightly different pixels (in BGR space) are still
// counted as a matching pixel by this simple manhattan distance threshold.
const int kPixelManhattanDistanceTolerance = 25;

std::string RunTestScript(base::StringPiece test_script,
                          content::WebContents* contents,
                          const std::string& element_id) {
  std::string script = base::StringPrintf(
      "var plugin = window.document.getElementById('%s');"
      "if (plugin === undefined ||"
      "    (plugin.nodeName !== 'OBJECT' && plugin.nodeName !== 'EMBED')) {"
      "  window.domAutomationController.send('error');"
      "} else {"
      "  %s"
      "}",
      element_id.c_str(), test_script.data());
  std::string result;
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractString(contents, script, &result));
  return result;
}

// This also tests that we have JavaScript access to the underlying plugin.
bool PluginLoaded(content::WebContents* contents,
                  const std::string& element_id) {
  std::string result = RunTestScript(
      "if (plugin.postMessage === undefined) {"
      "  window.domAutomationController.send('poster_only');"
      "} else {"
      "  window.domAutomationController.send('plugin_loaded');"
      "}",
      contents, element_id);
  EXPECT_NE("error", result);
  return result == "plugin_loaded";
}

// Blocks until the placeholder is ready.
void WaitForPlaceholderReady(content::WebContents* contents,
                             const std::string& element_id) {
  std::string result = RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data === 'placeholderReady') {"
      "    window.domAutomationController.send('ready');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.hasAttribute('placeholderReady')) {"
      "  window.domAutomationController.send('ready');"
      "  plugin.removeEventListener('message', handleEvent);"
      "}",
      contents, element_id);
  ASSERT_EQ("ready", result);
}

// Also waits for the placeholder UI overlay to finish loading.
void VerifyPluginIsThrottled(content::WebContents* contents,
                             const std::string& element_id) {
  std::string result = RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data.isPeripheral && event.data.isThrottled && "
      "      event.data.isHiddenForPlaceholder) {"
      "    window.domAutomationController.send('throttled');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.postMessage !== undefined) {"
      "  plugin.postMessage('getPowerSaverStatus');"
      "}",
      contents, element_id);
  EXPECT_EQ("throttled", result);

  // Page should continue to have JavaScript access to all throttled plugins.
  EXPECT_TRUE(PluginLoaded(contents, element_id));

  WaitForPlaceholderReady(contents, element_id);
}

void VerifyPluginMarkedEssential(content::WebContents* contents,
                                 const std::string& element_id) {
  std::string result = RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data.isPeripheral === false) {"
      "    window.domAutomationController.send('essential');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.postMessage !== undefined) {"
      "  plugin.postMessage('getPowerSaverStatus');"
      "}",
      contents, element_id);
  EXPECT_EQ("essential", result);
  EXPECT_TRUE(PluginLoaded(contents, element_id));
}

std::unique_ptr<net::test_server::HttpResponse> RespondWithHTML(
    const std::string& html,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_content_type("text/html");
  response->set_content(html);
  return std::move(response);
}

void VerifyVisualStateUpdated(const base::Closure& done_cb,
                              bool visual_state_updated) {
  ASSERT_TRUE(visual_state_updated);
  done_cb.Run();
}

bool SnapshotMatches(const base::FilePath& reference, const SkBitmap& bitmap) {
  if (bitmap.width() < kComparisonWidth ||
      bitmap.height() < kComparisonHeight) {
    return false;
  }

  std::string reference_data;
  if (!base::ReadFileToString(reference, &reference_data))
    return false;

  int w = 0;
  int h = 0;
  std::vector<unsigned char> decoded;
  if (!gfx::PNGCodec::Decode(
          reinterpret_cast<unsigned char*>(string_as_array(&reference_data)),
          reference_data.size(), gfx::PNGCodec::FORMAT_BGRA, &decoded, &w,
          &h)) {
    return false;
  }

  if (w < kComparisonWidth || h < kComparisonHeight)
    return false;

  int32_t* ref_pixels = reinterpret_cast<int32_t*>(decoded.data());
  SkAutoLockPixels lock_image(bitmap);
  int32_t* pixels = static_cast<int32_t*>(bitmap.getPixels());

  bool success = true;
  for (int y = 0; y < kComparisonHeight; ++y) {
    for (int x = 0; x < kComparisonWidth; ++x) {
      int32_t pixel = pixels[y * bitmap.rowBytes() / sizeof(int32_t) + x];
      int pixel_b = pixel & 0xFF;
      int pixel_g = (pixel >> 8) & 0xFF;
      int pixel_r = (pixel >> 16) & 0xFF;

      int32_t ref_pixel = ref_pixels[y * w + x];
      int ref_pixel_b = ref_pixel & 0xFF;
      int ref_pixel_g = (ref_pixel >> 8) & 0xFF;
      int ref_pixel_r = (ref_pixel >> 16) & 0xFF;

      int manhattan_distance = abs(pixel_b - ref_pixel_b) +
                               abs(pixel_g - ref_pixel_g) +
                               abs(pixel_r - ref_pixel_r);

      if (manhattan_distance > kPixelManhattanDistanceTolerance) {
        ADD_FAILURE() << "Pixel test failed on (" << x << ", " << y << "). " <<
            "Pixel manhattan distance: " << manhattan_distance << ".";
        success = false;
      }
    }
  }

  return success;
}

// |snapshot_matches| is set to true if the snapshot matches the reference and
// the test passes. Otherwise, set to false.
void CompareSnapshotToReference(const base::FilePath& reference,
                                bool* snapshot_matches,
                                const base::Closure& done_cb,
                                const SkBitmap& bitmap,
                                content::ReadbackResponse response) {
  DCHECK(snapshot_matches);
  ASSERT_EQ(content::READBACK_SUCCESS, response);

  *snapshot_matches = SnapshotMatches(reference, bitmap);

  // When rebaselining the pixel test, the test may fail. However, the
  // reference file will still be overwritten.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRebaselinePixelTests)) {
    SkBitmap clipped_bitmap;
    bitmap.extractSubset(&clipped_bitmap,
                         SkIRect::MakeWH(kComparisonWidth, kComparisonHeight));
    std::vector<unsigned char> png_data;
    ASSERT_TRUE(
        gfx::PNGCodec::EncodeBGRASkBitmap(clipped_bitmap, false, &png_data));
    ASSERT_EQ(static_cast<int>(png_data.size()),
              base::WriteFile(reference,
                              reinterpret_cast<const char*>(png_data.data()),
                              png_data.size()));
  }

  done_cb.Run();
}

}  // namespace

class PluginPowerSaverBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    if (PixelTestsEnabled())
      EnablePixelOutput();

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    embedded_test_server()->ServeFilesFromDirectory(
        ui_test_utils::GetTestFilePath(
            base::FilePath(FILE_PATH_LITERAL("plugin_power_saver")),
            base::FilePath()));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePepperTesting);
    command_line->AppendSwitch(switches::kEnablePluginPlaceholderTesting);
    command_line->AppendSwitchASCII(
        switches::kOverridePluginPowerSaverForTesting, "ignore-list");

    ASSERT_TRUE(ppapi::RegisterPowerSaverTestPlugin(command_line));

    // Allows us to use the same reference image on HiDPI/Retina displays.
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");

    // The pixel tests run more reliably in software mode.
    if (PixelTestsEnabled())
      command_line->AppendSwitch(switches::kDisableGpu);
  }

 protected:
  void LoadHTML(const std::string& html) {
    if (PixelTestsEnabled()) {
      gfx::Rect bounds(gfx::Rect(0, 0, kBrowserWidth, kBrowserHeight));
      gfx::Rect screen_bounds =
          display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
      ASSERT_GT(screen_bounds.width(), kBrowserWidth);
      ASSERT_GT(screen_bounds.height(), kBrowserHeight);
      browser()->window()->SetBounds(bounds);
    }

    ASSERT_TRUE(embedded_test_server()->Started());
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&RespondWithHTML, html));
    ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
    EXPECT_TRUE(content::WaitForRenderFrameReady(
        GetActiveWebContents()->GetMainFrame()));
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // This sends a simulated click at |point| and waits for test plugin to send
  // a status message indicating that it is essential. The test plugin sends a
  // status message during:
  //  - Plugin creation, to handle a plugin freshly created from a poster.
  //  - Peripheral status change.
  //  - In response to the explicit 'getPowerSaverStatus' request, in case the
  //    test has missed the above two events.
  void SimulateClickAndAwaitMarkedEssential(const std::string& element_id,
                                            const gfx::Point& point) {
    WaitForPlaceholderReady(GetActiveWebContents(), element_id);
    content::SimulateMouseClickAt(GetActiveWebContents(), 0 /* modifiers */,
                                  blink::WebMouseEvent::ButtonLeft, point);

    VerifyPluginMarkedEssential(GetActiveWebContents(), element_id);
  }

  // |element_id| must be an element on the foreground tab.
  void VerifyPluginIsPosterOnly(const std::string& element_id) {
    EXPECT_FALSE(PluginLoaded(GetActiveWebContents(), element_id));
    WaitForPlaceholderReady(GetActiveWebContents(), element_id);
  }

  bool VerifySnapshot(const base::FilePath::StringType& expected_filename) {
    if (!PixelTestsEnabled())
      return true;

    base::FilePath reference = ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("plugin_power_saver")),
        base::FilePath(expected_filename));

    GetActiveWebContents()->GetMainFrame()->InsertVisualStateCallback(
        base::Bind(&VerifyVisualStateUpdated,
                   base::MessageLoop::QuitWhenIdleClosure()));
    content::RunMessageLoop();

    content::RenderWidgetHost* rwh =
        GetActiveWebContents()->GetRenderViewHost()->GetWidget();

    if (!rwh->CanCopyFromBackingStore()) {
      ADD_FAILURE() << "Could not copy from backing store.";
      return false;
    }

    bool snapshot_matches = false;
    rwh->CopyFromBackingStore(
        gfx::Rect(), gfx::Size(),
        base::Bind(&CompareSnapshotToReference, reference, &snapshot_matches,
                   base::MessageLoop::QuitWhenIdleClosure()),
        kN32_SkColorType);

    content::RunMessageLoop();

    return snapshot_matches;
  }

  // TODO(tommycli): Remove this once all flakiness resolved.
  bool PixelTestsEnabled() {
#if defined(OS_WIN) || defined(ADDRESS_SANITIZER)
    // Flaky on Windows and Asan bots. See crbug.com/549285.
    return false;
#elif defined(OS_CHROMEOS)
    // Because ChromeOS cannot use software rendering and the pixel tests
    // continue to flake with hardware acceleration, disable these on ChromeOS.
    return false;
#else
    return true;
#endif
  }
};

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, EssentialPlugins) {
  LoadHTML(
      "<object id='small_same_origin' data='fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='100'>"
      "</object>"
      "<object id='small_same_origin_poster' data='fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='100' "
      "    poster='click_me.png'>"
      "</object>"
      "<object id='tiny_cross_origin_1' data='http://a.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='3' height='3'>"
      "</object>"
      "<object id='tiny_cross_origin_2' data='http://a.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='1' height='1'>"
      "</object>"
      "<object id='large_cross_origin' data='http://b.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='500'>"
      "</object>"
      "<object id='medium_16_9_cross_origin' data='http://c.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='480' height='270'>"
      "</object>");

  VerifyPluginMarkedEssential(GetActiveWebContents(), "small_same_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "small_same_origin_poster");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "tiny_cross_origin_1");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "tiny_cross_origin_2");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "large_cross_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "medium_16_9_cross_origin");
}

// Flaky on WebKit Mac dbg bots: crbug.com/599484.
#if defined(OS_MACOSX)
#define MAYBE_SmallCrossOrigin DISABLED_SmallCrossOrigin
#else
#define MAYBE_SmallCrossOrigin SmallCrossOrigin
#endif
IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, MAYBE_SmallCrossOrigin) {
  LoadHTML(
      "<object id='plugin' data='http://otherorigin.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='100'>"
      "</object>"
      "<br>"
      "<object id='plugin_poster' data='http://otherorigin.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='100' "
      "    poster='click_me.png'>"
      "</object>");

  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
  VerifyPluginIsPosterOnly("plugin_poster");

  EXPECT_TRUE(
      VerifySnapshot(FILE_PATH_LITERAL("small_cross_origin_expected.png")));

  SimulateClickAndAwaitMarkedEssential("plugin", gfx::Point(50, 50));
  SimulateClickAndAwaitMarkedEssential("plugin_poster", gfx::Point(50, 150));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, SmallerThanPlayIcon) {
  LoadHTML(
      "<object id='plugin_16' type='application/x-ppapi-tests' "
      "    width='16' height='16'></object>"
      "<object id='plugin_32' type='application/x-ppapi-tests' "
      "    width='32' height='32'></object>"
      "<object id='plugin_16_64' type='application/x-ppapi-tests' "
      "    width='16' height='64'></object>"
      "<object id='plugin_64_16' type='application/x-ppapi-tests' "
      "    width='64' height='16'></object>");

  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_16");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_32");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_16_64");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_64_16");

  EXPECT_TRUE(
      VerifySnapshot(FILE_PATH_LITERAL("smaller_than_play_icon_expected.png")));
}

// Flaky on WebKit Mac dbg bots: crbug.com/599484.
#if defined(OS_MACOSX)
#define MAYBE_PosterTests DISABLED_PosterTests
#else
#define MAYBE_PosterTests PosterTests
#endif
IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, MAYBE_PosterTests) {
  // This test simultaneously verifies the varied supported poster syntaxes,
  // as well as verifies that the poster is rendered correctly with various
  // mismatched aspect ratios and sizes, following the same rules as VIDEO.
  LoadHTML(
      "<object id='plugin_src' type='application/x-ppapi-tests' "
      "    width='100' height='100' poster='click_me.png'></object>"
      "<object id='plugin_srcset' type='application/x-ppapi-tests' "
      "    width='100' height='100' "
      "    poster='click_me.png 1x, click_me.png 2x'></object>"
      "<br>"

      "<object id='plugin_poster_param' type='application/x-ppapi-tests' "
      "    width='100' height='100'>"
      "  <param name='poster' value='click_me.png 1x, click_me.png 2x'>"
      "</object>"
      "<embed id='plugin_embed_src' type='application/x-ppapi-tests' "
      "    width='100' height='100' poster='click_me.png'></embed>"
      "<embed id='plugin_embed_srcset' type='application/x-ppapi-tests' "
      "    width='100' height='100'"
      "    poster='click_me.png 1x, click_me.png 2x'></embed>"
      "<br>"

      "<object id='poster_missing' type='application/x-ppapi-tests' "
      "    width='100' height='100' poster='missing.png'></object>"
      "<object id='poster_too_small' type='application/x-ppapi-tests' "
      "    width='100' height='50' poster='click_me.png'></object>"
      "<object id='poster_too_big' type='application/x-ppapi-tests' "
      "    width='100' height='150' poster='click_me.png'></object>"
      "<br>"

      "<object id='poster_16' type='application/x-ppapi-tests' "
      "    width='16' height='16' poster='click_me.png'></object>"
      "<object id='poster_32' type='application/x-ppapi-tests' "
      "    width='32' height='32' poster='click_me.png'></object>"
      "<object id='poster_16_64' type='application/x-ppapi-tests' "
      "    width='16' height='64' poster='click_me.png'></object>"
      "<object id='poster_64_16' type='application/x-ppapi-tests' "
      "    width='64' height='16' poster='click_me.png'></object>"
      "<br>"

      "<div id='container' "
      "    style='width: 400px; height: 100px; overflow: hidden;'>"
      "  <object id='poster_obscured' data='http://otherorigin.com/fake.swf' "
      "      type='application/x-ppapi-tests' width='400' height='500' "
      "      poster='click_me.png'>"
      "  </object>"
      "</div>");

  VerifyPluginIsPosterOnly("plugin_src");
  VerifyPluginIsPosterOnly("plugin_srcset");

  VerifyPluginIsPosterOnly("plugin_poster_param");
  VerifyPluginIsPosterOnly("plugin_embed_src");
  VerifyPluginIsPosterOnly("plugin_embed_srcset");

  VerifyPluginIsPosterOnly("poster_missing");
  VerifyPluginIsPosterOnly("poster_too_small");
  VerifyPluginIsPosterOnly("poster_too_big");

  VerifyPluginIsPosterOnly("poster_16");
  VerifyPluginIsPosterOnly("poster_32");
  VerifyPluginIsPosterOnly("poster_16_64");
  VerifyPluginIsPosterOnly("poster_64_16");

  VerifyPluginIsPosterOnly("poster_obscured");

  EXPECT_TRUE(VerifySnapshot(FILE_PATH_LITERAL("poster_tests_expected.png")));

  // Test that posters can be unthrottled via click.
  SimulateClickAndAwaitMarkedEssential("plugin_src", gfx::Point(50, 50));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargePostersNotThrottled) {
  // This test verifies that small posters are throttled, large posters are not,
  // and that large posters can whitelist origins for other plugins.
  LoadHTML(
      "<object id='poster_small' data='http://a.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='50' height='50' "
      "    poster='click_me.png'></object>"
      "<object id='poster_whitelisted_origin' data='http://b.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='50' height='50' "
      "    poster='click_me.png'></object>"
      "<object id='plugin_whitelisted_origin' data='http://b.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='50' height='50'></object>"
      "<br>"
      "<object id='poster_large' data='http://b.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='300' "
      "    poster='click_me.png'></object>");

  VerifyPluginIsPosterOnly("poster_small");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "poster_whitelisted_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "plugin_whitelisted_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "poster_large");
}

// Flaky on ASAN bots: crbug.com/560765.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_OriginWhitelisting DISABLED_OriginWhitelisting
#else
#define MAYBE_OriginWhitelisting OriginWhitelisting
#endif
IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, MAYBE_OriginWhitelisting) {
  LoadHTML(
      "<object id='plugin_small' data='http://a.com/fake1.swf' "
      "    type='application/x-ppapi-tests' width='100' height='100'></object>"
      "<object id='plugin_small_poster' data='http://a.com/fake1.swf' "
      "    type='application/x-ppapi-tests' width='100' height='100' "
      "    poster='click_me.png'></object>"
      "<object id='plugin_large' data='http://a.com/fake2.swf' "
      "    type='application/x-ppapi-tests' width='400' height='500'>"
      "</object>");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_small");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_small_poster");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_large");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargeCrossOriginObscured) {
  LoadHTML(
      "<div id='container' "
      "    style='width: 100px; height: 400px; overflow: hidden;'>"
      "  <object id='plugin' data='http://otherorigin.com/fake.swf' "
      "      type='application/x-ppapi-tests' width='400' height='500' "
      "      style='float: right;'>"
      "  </object>"
      "</div>");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
  EXPECT_TRUE(VerifySnapshot(
      FILE_PATH_LITERAL("large_cross_origin_obscured_expected.png")));

  // Test that's unthrottled if it is unobscured.
  std::string script =
      "var container = window.document.getElementById('container');"
      "container.setAttribute('style', 'width: 400px; height: 400px;');";
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), script));
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ExpandingSmallPlugin) {
  LoadHTML(
      "<object id='plugin' data='http://otherorigin.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='80'></object>");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");

  std::string script = "window.document.getElementById('plugin').height = 400;";
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), script));
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, BackgroundTabPlugins) {
  std::string html =
      "<object id='same_origin' data='fake.swf' "
      "    type='application/x-ppapi-tests'></object>"
      "<object id='small_cross_origin' data='http://otherorigin.com/fake1.swf' "
      "    type='application/x-ppapi-tests' width='400' height='100'></object>";
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&RespondWithHTML, html));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->base_url(), NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* background_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(
      content::WaitForRenderFrameReady(background_contents->GetMainFrame()));

  EXPECT_FALSE(PluginLoaded(background_contents, "same_origin"));
  EXPECT_FALSE(PluginLoaded(background_contents, "small_cross_origin"));

  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_EQ(background_contents, GetActiveWebContents());

  VerifyPluginMarkedEssential(background_contents, "same_origin");
  VerifyPluginIsThrottled(background_contents, "small_cross_origin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ZoomIndependent) {
  ui_zoom::ZoomController::FromWebContents(GetActiveWebContents())
      ->SetZoomLevel(4.0);
  LoadHTML(
      "<object id='plugin' data='http://otherorigin.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='200'>"
      "</object>");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
}
