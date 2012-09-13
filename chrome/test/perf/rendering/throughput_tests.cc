// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "base/values.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/perf/browser_perf_test.h"
#include "chrome/test/perf/perf_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/test/gpu/gpu_test_config.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

enum RunTestFlags {
  kNone = 0,
  kInternal = 1 << 0,         // Test uses internal test data.
  kAllowExternalDNS = 1 << 1, // Test needs external DNS lookup.
  kIsGpuCanvasTest = 1 << 2   // Test uses GPU accelerated canvas features.
};

enum ThroughputTestFlags {
  kSW = 0,
  kGPU = 1 << 0,
  kCompositorThread = 1 << 1
};

const int kSpinUpTimeMs = 4 * 1000;
const int kRunTimeMs = 10 * 1000;
const int kIgnoreSomeFrames = 3;

class ThroughputTest : public BrowserPerfTest {
 public:
  explicit ThroughputTest(int flags) :
      use_gpu_(flags & kGPU),
      use_compositor_thread_(flags & kCompositorThread),
      spinup_time_ms_(kSpinUpTimeMs),
      run_time_ms_(kRunTimeMs) {}

  // This indicates running on GPU bots, not necessarily using the GPU.
  bool IsGpuAvailable() const {
    return CommandLine::ForCurrentProcess()->HasSwitch("enable-gpu");
  }

  // Parse flags from JSON to control the test behavior.
  bool ParseFlagsFromJSON(const FilePath& json_dir,
                          const std::string& json,
                          int index) {
    scoped_ptr<base::Value> root;
    root.reset(base::JSONReader::Read(json));

    ListValue* root_list = NULL;
    if (!root.get() || !root->GetAsList(&root_list)) {
      LOG(ERROR) << "JSON missing root list element";
      return false;
    }

    DictionaryValue* item = NULL;
    if (!root_list->GetDictionary(index, &item)) {
      LOG(ERROR) << "index " << index << " not found in JSON";
      return false;
    }

    std::string str;
    if (item->GetStringASCII("url", &str)) {
      gurl_ = GURL(str);
    } else if (item->GetStringASCII("file", &str)) {
      FilePath empty;
      gurl_ = URLFixerUpper::FixupRelativeFile(empty, empty.AppendASCII(str));
    } else {
      LOG(ERROR) << "missing url or file";
      return false;
    }

    if (!gurl_.is_valid()) {
      LOG(ERROR) << "invalid url: " << gurl_.possibly_invalid_spec();
      return false;
    }

    FilePath::StringType cache_dir;
    if (item->GetString("local_path", &cache_dir))
      local_cache_path_ = json_dir.Append(cache_dir);

    int num;
    if (item->GetInteger("spinup_time", &num))
      spinup_time_ms_ = num * 1000;
    if (item->GetInteger("run_time", &num))
      run_time_ms_ = num * 1000;

    DictionaryValue* pixel = NULL;
    if (item->GetDictionary("wait_pixel", &pixel)) {
      int x, y, r, g, b;
      ListValue* color;
      if (pixel->GetInteger("x", &x) &&
          pixel->GetInteger("y", &y) &&
          pixel->GetString("op", &str) &&
          pixel->GetList("color", &color) &&
          color->GetInteger(0, &r) &&
          color->GetInteger(1, &g) &&
          color->GetInteger(2, &b)) {
        wait_for_pixel_.reset(new WaitPixel(x, y, r, g, b, str));
      } else {
        LOG(ERROR) << "invalid wait_pixel args";
        return false;
      }
    }
    return true;
  }

  // Parse extra-chrome-flags for extra command line flags.
  void ParseFlagsFromCommandLine() {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kExtraChromeFlags))
      return;
    CommandLine::StringType flags =
        CommandLine::ForCurrentProcess()->GetSwitchValueNative(
            switches::kExtraChromeFlags);
    if (MatchPattern(flags, FILE_PATH_LITERAL("*.json:*"))) {
      CommandLine::StringType::size_type colon_pos = flags.find_last_of(':');
      CommandLine::StringType::size_type num_pos = colon_pos + 1;
      int index;
      ASSERT_TRUE(base::StringToInt(
          flags.substr(num_pos, flags.size() - num_pos), &index));
      FilePath filepath(flags.substr(0, colon_pos));
      std::string json;
      ASSERT_TRUE(file_util::ReadFileToString(filepath, &json));
      ASSERT_TRUE(ParseFlagsFromJSON(filepath.DirName(), json, index));
    } else {
      gurl_ = GURL(flags);
    }
  }

  void AllowExternalDNS() {
    net::RuleBasedHostResolverProc* resolver =
        new net::RuleBasedHostResolverProc(host_resolver());
    resolver->AllowDirectLookup("*");
    host_resolver_override_.reset(
        new net::ScopedDefaultHostResolverProc(resolver));
  }

  void ResetAllowExternalDNS() {
    host_resolver_override_.reset();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    BrowserPerfTest::SetUpCommandLine(command_line);
    ParseFlagsFromCommandLine();
    if (!local_cache_path_.value().empty()) {
      // If --record-mode is already specified, don't set playback-mode.
      if (!command_line->HasSwitch(switches::kRecordMode))
        command_line->AppendSwitch(switches::kPlaybackMode);
      command_line->AppendSwitchNative(switches::kDiskCacheDir,
                                       local_cache_path_.value());
      LOG(INFO) << local_cache_path_.value();
    }
    // We are measuring throughput, so we don't want any FPS throttling.
    command_line->AppendSwitch(switches::kDisableGpuVsync);
    command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);
    // Enable or disable GPU acceleration.
    if (use_gpu_) {
      command_line->AppendSwitch(switches::kForceCompositingMode);
    } else {
      command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
      command_line->AppendSwitch(switches::kDisableExperimentalWebGL);
      command_line->AppendSwitch(switches::kDisableAccelerated2dCanvas);
    }
    if (use_compositor_thread_) {
      ASSERT_TRUE(use_gpu_);
      command_line->AppendSwitch(switches::kEnableThreadedCompositing);
    } else {
      command_line->AppendSwitch(switches::kDisableThreadedCompositing);
    }
  }

  void Wait(int ms) {
    base::RunLoop run_loop;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(ms));
    content::RunThisRunLoop(&run_loop);
  }

  // Take snapshot of the current tab, encode it as PNG, and save to a SkBitmap.
  bool TabSnapShotToImage(SkBitmap* bitmap) {
    CHECK(bitmap);
    std::vector<unsigned char> png;

    gfx::Rect root_bounds = browser()->window()->GetBounds();
    gfx::Rect tab_contents_bounds;
    chrome::GetActiveWebContents(browser())->GetContainerBounds(
        &tab_contents_bounds);

    gfx::Rect snapshot_bounds(tab_contents_bounds.x() - root_bounds.x(),
                              tab_contents_bounds.y() - root_bounds.y(),
                              tab_contents_bounds.width(),
                              tab_contents_bounds.height());

    gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
    if (!chrome::GrabWindowSnapshotForUser(native_window, &png,
                                           snapshot_bounds)) {
      LOG(ERROR) << "browser::GrabWindowSnapShot() failed";
      return false;
    }

    if (!gfx::PNGCodec::Decode(reinterpret_cast<unsigned char*>(&*png.begin()),
                               png.size(), bitmap)) {
      LOG(ERROR) << "Decode PNG to a SkBitmap failed";
      return false;
    }
    return true;
  }

  // Check a pixel color every second until it passes test.
  void WaitForPixelColor() {
    if (wait_for_pixel_.get()) {
      bool success = false;
      do {
        SkBitmap bitmap;
        ASSERT_TRUE(TabSnapShotToImage(&bitmap));
        success = wait_for_pixel_->IsDone(bitmap);
        if (!success)
          Wait(1000);
      } while (!success);
    }
  }

  // flags is one or more of RunTestFlags OR'd together.
  void RunTest(const std::string& test_name, int flags) {
    // Set path to test html.
    FilePath test_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_path));
    test_path = test_path.Append(FILE_PATH_LITERAL("perf"));
    if (flags & kInternal)
      test_path = test_path.Append(FILE_PATH_LITERAL("private"));
    test_path = test_path.Append(FILE_PATH_LITERAL("rendering"));
    test_path = test_path.Append(FILE_PATH_LITERAL("throughput"));
    test_path = test_path.AppendASCII(test_name);
    test_path = test_path.Append(FILE_PATH_LITERAL("index.html"));
    ASSERT_TRUE(file_util::PathExists(test_path))
        << "Missing test file: " << test_path.value();

    gurl_ = net::FilePathToFileURL(test_path);
    RunTestWithURL(test_name, flags);
  }

  // flags is one or more of RunTestFlags OR'd together.
  void RunCanvasBenchTest(const std::string& test_name, int flags) {
    // Set path to test html.
    FilePath test_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_path));
    test_path = test_path.Append(FILE_PATH_LITERAL("perf"));
    test_path = test_path.Append(FILE_PATH_LITERAL("canvas_bench"));
    test_path = test_path.AppendASCII(test_name + ".html");
    ASSERT_TRUE(file_util::PathExists(test_path))
        << "Missing test file: " << test_path.value();

    gurl_ = net::FilePathToFileURL(test_path);
    RunTestWithURL(test_name, flags);
  }

  // flags is one or more of RunTestFlags OR'd together.
  void RunTestWithURL(int flags) {
    RunTestWithURL(gurl_.possibly_invalid_spec(), flags);
  }

  // flags is one or more of RunTestFlags OR'd together.
  void RunTestWithURL(const std::string& test_name, int flags) {
    using trace_analyzer::Query;
    using trace_analyzer::TraceAnalyzer;
    using trace_analyzer::TraceEventVector;

#if defined(OS_WIN)
    if (use_gpu_ && (flags & kIsGpuCanvasTest) &&
        base::win::OSInfo::GetInstance()->version() == base::win::VERSION_XP) {
      // crbug.com/128208
      LOG(WARNING) << "Test skipped: GPU canvas tests do not run on XP.";
      return;
    }
#endif

    if (use_gpu_ && !IsGpuAvailable()) {
      LOG(WARNING) << "Test skipped: requires gpu. Pass --enable-gpu on the "
                      "command line if use of GPU is desired.";
      return;
    }

    if (flags & kAllowExternalDNS)
      AllowExternalDNS();

    std::string json_events;
    TraceEventVector events_sw, events_gpu;
    scoped_ptr<TraceAnalyzer> analyzer;

    LOG(INFO) << gurl_.possibly_invalid_spec();
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), gurl_, CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
    content::WaitForLoadStop(chrome::GetActiveWebContents(browser()));

    // Let the test spin up.
    LOG(INFO) << "Spinning up test...";
    ASSERT_TRUE(tracing::BeginTracing("test_gpu"));
    Wait(spinup_time_ms_);
    ASSERT_TRUE(tracing::EndTracing(&json_events));

    // Wait for a pixel color to change (if requested).
    WaitForPixelColor();

    // Check if GPU is rendering:
    analyzer.reset(TraceAnalyzer::Create(json_events));
    bool ran_on_gpu = (analyzer->FindEvents(
        Query::EventNameIs("SwapBuffers"), &events_gpu) > 0u);
    LOG(INFO) << "Mode: " << (ran_on_gpu ? "GPU" : "Software");
    EXPECT_EQ(use_gpu_, ran_on_gpu);

    // Let the test run for a while.
    LOG(INFO) << "Running test...";
    ASSERT_TRUE(tracing::BeginTracing("test_fps"));
    Wait(run_time_ms_);
    ASSERT_TRUE(tracing::EndTracing(&json_events));

    // Search for frame ticks. We look for both SW and GPU frame ticks so that
    // the test can verify that only one or the other are found.
    analyzer.reset(TraceAnalyzer::Create(json_events));
    Query query_sw = Query::EventNameIs("TestFrameTickSW");
    Query query_gpu = Query::EventNameIs("TestFrameTickGPU");
    analyzer->FindEvents(query_sw, &events_sw);
    analyzer->FindEvents(query_gpu, &events_gpu);
    TraceEventVector* frames = NULL;
    if (use_gpu_) {
      frames = &events_gpu;
      EXPECT_EQ(0u, events_sw.size());
    } else {
      frames = &events_sw;
      EXPECT_EQ(0u, events_gpu.size());
    }
    ASSERT_GT(frames->size(), 20u);
    // Cull a few leading and trailing events as they might be unreliable.
    TraceEventVector rate_events(frames->begin() + kIgnoreSomeFrames,
                                 frames->end() - kIgnoreSomeFrames);
    trace_analyzer::RateStats stats;
    ASSERT_TRUE(GetRateStats(rate_events, &stats, NULL));
    LOG(INFO) << "FPS = " << 1000000.0 / stats.mean_us;

    // Print perf results.
    double mean_ms = stats.mean_us / 1000.0;
    double std_dev_ms = stats.standard_deviation_us / 1000.0 / 1000.0;
    std::string trace_name = use_compositor_thread_? "gpu_thread" :
                             ran_on_gpu ? "gpu" : "software";
    std::string mean_and_error = base::StringPrintf("%f,%f", mean_ms,
                                                    std_dev_ms);
    perf_test::PrintResultMeanAndError(test_name, "", trace_name,
                                       mean_and_error, "frame_time", true);

    if (flags & kAllowExternalDNS)
      ResetAllowExternalDNS();

    // Close the tab so that we can quit without timing out during the
    // wait-for-idle stage in browser_test framework.
    chrome::GetActiveWebContents(browser())->Close();
  }

 private:
  // WaitPixel checks a color against the color at the given pixel coordinates
  // of an SkBitmap.
  class WaitPixel {
   enum Operator {
     EQUAL,
     NOT_EQUAL
   };
   public:
    WaitPixel(int x, int y, int r, int g, int b, const std::string& op) :
        x_(x), y_(y), r_(r), g_(g), b_(b) {
      if (op == "!=")
        op_ = EQUAL;
      else if (op == "==")
        op_ = NOT_EQUAL;
      else
        CHECK(false) << "op value \"" << op << "\" is not supported";
    }
    bool IsDone(const SkBitmap& bitmap) {
      SkColor color = bitmap.getColor(x_, y_);
      int r = SkColorGetR(color);
      int g = SkColorGetG(color);
      int b = SkColorGetB(color);
      LOG(INFO) << "color("  << x_ << "," << y_ << "): " <<
                   r << "," << g << "," << b;
      switch (op_) {
        case EQUAL:
          return r != r_ || g != g_ || b != b_;
        case NOT_EQUAL:
          return r == r_ && g == g_ && b == b_;
        default:
          return false;
      }
    }
   private:
    int x_;
    int y_;
    int r_;
    int g_;
    int b_;
    Operator op_;
  };

  bool use_gpu_;
  bool use_compositor_thread_;
  int spinup_time_ms_;
  int run_time_ms_;
  FilePath local_cache_path_;
  GURL gurl_;
  scoped_ptr<net::ScopedDefaultHostResolverProc> host_resolver_override_;
  scoped_ptr<WaitPixel> wait_for_pixel_;
};

// For running tests on GPU:
class ThroughputTestGPU : public ThroughputTest {
 public:
  ThroughputTestGPU() : ThroughputTest(kGPU) {}
};

// For running tests on GPU with the compositor thread:
class ThroughputTestThread : public ThroughputTest {
 public:
  ThroughputTestThread() : ThroughputTest(kGPU | kCompositorThread) {}
};

// For running tests on Software:
class ThroughputTestSW : public ThroughputTest {
 public:
  ThroughputTestSW() : ThroughputTest(kSW) {}
};

////////////////////////////////////////////////////////////////////////////////
/// Tests

// Run this test with a URL on the command line:
// performance_browser_tests --gtest_also_run_disabled_tests --enable-gpu
//     --gtest_filter=ThroughputTest*URL --extra-chrome-flags=http://...
// or, specify a json file with a list of tests, and the index of the test:
//     --extra-chrome-flags=path/to/tests.json:0
// The json format is an array of tests, for example:
// [
// {"url":"http://...",
//  "spinup_time":5,
//  "run_time":10,
//  "local_path":"path/to/disk-cache-dir",
//  "wait_pixel":{"x":10,"y":10,"op":"!=","color":[24,24,24]}},
// {"url":"http://..."}
// ]
// The only required argument is url. If local_path is set, the test goes into
// playback-mode and only loads files from the specified cache. If wait_pixel is
// specified, then after spinup_time the test waits for the color at the
// specified pixel coordinate to match the given color with the given operator.
IN_PROC_BROWSER_TEST_F(ThroughputTestSW, DISABLED_TestURL) {
  RunTestWithURL(kAllowExternalDNS);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, DISABLED_TestURL) {
  RunTestWithURL(kAllowExternalDNS);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, Particles) {
  RunTest("particles", kInternal);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestThread, Particles) {
  RunTest("particles", kInternal);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestSW, CanvasDemoSW) {
  RunTest("canvas-demo", kInternal);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, CanvasDemoGPU) {
  RunTest("canvas-demo", kInternal | kIsGpuCanvasTest);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestThread, CanvasDemoGPU) {
  RunTest("canvas-demo", kInternal | kIsGpuCanvasTest);
}

// CompositingHugeDivSW timed out on Mac Intel Release GPU bot
// See crbug.com/114781
// Stopped producing results in SW: crbug.com/127621
IN_PROC_BROWSER_TEST_F(ThroughputTestSW, DISABLED_CompositingHugeDivSW) {
  RunTest("compositing_huge_div", kNone);
}

// Failing with insufficient frames: crbug.com/127595
IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, DISABLED_CompositingHugeDivGPU) {
  RunTest("compositing_huge_div", kNone);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestSW, DrawImageShadowSW) {
  RunTest("canvas2d_balls_with_shadow", kNone);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, DrawImageShadowGPU) {
  RunTest("canvas2d_balls_with_shadow", kNone | kIsGpuCanvasTest);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestThread, DrawImageShadowGPU) {
  RunTest("canvas2d_balls_with_shadow", kNone | kIsGpuCanvasTest);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestSW, CanvasToCanvasDrawSW) {
  if (IsGpuAvailable() &&
      GPUTestBotConfig::CurrentConfigMatches("MAC AMD"))
    return;
  RunTest("canvas2d_balls_draw_from_canvas", kNone);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, CanvasToCanvasDrawGPU) {
  if (IsGpuAvailable() &&
      GPUTestBotConfig::CurrentConfigMatches("MAC AMD"))
    return;
  RunTest("canvas2d_balls_draw_from_canvas", kNone | kIsGpuCanvasTest);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestSW, CanvasTextSW) {
  if (IsGpuAvailable() &&
      GPUTestBotConfig::CurrentConfigMatches("MAC AMD"))
    return;
  RunTest("canvas2d_balls_text", kNone);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, CanvasTextGPU) {
  RunTest("canvas2d_balls_text", kNone | kIsGpuCanvasTest);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestSW, CanvasFillPathSW) {
  RunTest("canvas2d_balls_fill_path", kNone);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, CanvasFillPathGPU) {
  RunTest("canvas2d_balls_fill_path", kNone | kIsGpuCanvasTest);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestSW, CanvasSingleImageSW) {
  RunCanvasBenchTest("single_image", kNone);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, CanvasSingleImageGPU) {
  if (IsGpuAvailable() &&
      GPUTestBotConfig::CurrentConfigMatches("MAC AMD"))
    return;
  RunCanvasBenchTest("single_image", kNone | kIsGpuCanvasTest);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestSW, CanvasManyImagesSW) {
  RunCanvasBenchTest("many_images", kNone);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestGPU, CanvasManyImagesGPU) {
  RunCanvasBenchTest("many_images", kNone | kIsGpuCanvasTest);
}

IN_PROC_BROWSER_TEST_F(ThroughputTestThread, CanvasManyImagesGPU) {
  RunCanvasBenchTest("many_images", kNone | kIsGpuCanvasTest);
}

}  // namespace
