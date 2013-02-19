// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "base/version.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_info.h"
#include "content/public/test/browser_test_utils.h"
#include "content/test/gpu/gpu_test_config.h"
#include "net/base/net_util.h"
#include "ui/gl/gl_switches.h"
#include "ui/compositor/compositor_setup.h"
#if defined(OS_MACOSX)
#include "ui/surface/io_surface_support_mac.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using content::GpuDataManager;
using content::GpuFeatureType;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEventVector;

namespace {

const char kSwapBuffersEvent[] = "SwapBuffers";
const char kAcceleratedCanvasCreationEvent[] = "Canvas2DLayerBridgeCreation";
const char kWebGLCreationEvent[] = "DrawingBufferCreation";

class GpuFeatureTest : public InProcessBrowserTest {
 public:
  GpuFeatureTest() : trace_categories_("test_gpu"), gpu_enabled_(false) {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    base::FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Do not use mesa if real GPU is required.
    if (!command_line->HasSwitch(switches::kUseGpuInTests)) {
#if !defined(OS_MACOSX)
      CHECK(test_launcher_utils::OverrideGLImplementation(
          command_line, gfx::kGLImplementationOSMesaName)) <<
          "kUseGL must not be set by test framework code!";
#endif
    } else {
      gpu_enabled_ = true;
    }
    command_line->AppendSwitch(switches::kDisablePopupBlocking);
    ui::DisableTestCompositor();
    command_line->AppendSwitchASCII(switches::kWindowSize, "400,300");
  }

  void SetupBlacklist(const std::string& json_blacklist) {
    content::GPUInfo gpu_info;
    GpuDataManager::GetInstance()->InitializeForTesting(
        json_blacklist, gpu_info);
  }

  // If expected_reply is NULL, we don't check the reply content.
  void RunTest(const base::FilePath& url,
               const char* expected_reply,
               bool new_tab) {
#if defined(OS_LINUX) && !defined(NDEBUG)
    // Bypass tests on GPU Linux Debug bots.
    if (gpu_enabled_)
      return;
#endif

    base::FilePath test_path;
    test_path = gpu_test_dir_.Append(url);
    ASSERT_TRUE(file_util::PathExists(test_path))
        << "Missing test file: " << test_path.value();

    content::DOMMessageQueue message_queue;
    if (new_tab) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), net::FilePathToFileURL(test_path),
          NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_NONE);
    } else {
      ui_test_utils::NavigateToURL(
          browser(), net::FilePathToFileURL(test_path));
    }

    std::string result;
    // Wait for message indicating the test has finished running.
    ASSERT_TRUE(message_queue.WaitForMessage(&result));
    if (expected_reply)
      EXPECT_STREQ(expected_reply, result.c_str());
  }

  // Open the URL and check the trace stream for the given event.
  void RunEventTest(const base::FilePath& url,
                    const char* event_name = NULL,
                    bool event_expected = false) {
#if defined(OS_LINUX) && !defined(NDEBUG)
    // Bypass tests on GPU Linux Debug bots.
    if (gpu_enabled_)
      return;
#endif
#if defined(OS_MACOSX)
    // Bypass tests on Mac OSX 10.5 bots (IOSurfaceSupport is now required).
    if (!IOSurfaceSupport::Initialize())
      return;
#endif

    ASSERT_TRUE(tracing::BeginTracing(trace_categories_));

    // Have to use a new tab for the blacklist to work.
    RunTest(url, NULL, true);

    ASSERT_TRUE(tracing::EndTracing(&trace_events_json_));

    analyzer_.reset(TraceAnalyzer::Create(trace_events_json_));
    analyzer_->AssociateBeginEndEvents();
    TraceEventVector events;

    if (!event_name)
      return;

    size_t event_count =
        analyzer_->FindEvents(Query::EventNameIs(event_name), &events);

    if (event_expected)
      EXPECT_GT(event_count, 0U);
    else
      EXPECT_EQ(event_count, 0U);
  }

  // Trigger a resize of the chrome window, and use tracing to wait for the
  // given |wait_event|.
  bool ResizeAndWait(const gfx::Rect& new_bounds,
                     const char* trace_categories,
                     const char* wait_category,
                     const char* wait_event) {
    if (!tracing::BeginTracingWithWatch(trace_categories, wait_category,
                                        wait_event, 1))
      return false;
    browser()->window()->SetBounds(new_bounds);
    if (!tracing::WaitForWatchEvent(base::TimeDelta()))
      return false;
    if (!tracing::EndTracing(&trace_events_json_))
      return false;
    analyzer_.reset(TraceAnalyzer::Create(trace_events_json_));
    analyzer_->AssociateBeginEndEvents();
    return true;
  }

 protected:
  base::FilePath gpu_test_dir_;
  scoped_ptr<TraceAnalyzer> analyzer_;
  std::string trace_categories_;
  std::string trace_events_json_;
  bool gpu_enabled_;
};

#if defined(OS_WIN)
// This test is flaky on Windows. http://crbug.com/177113
#define MAYBE_AcceleratedCompositingAllowed DISABLED_AcceleratedCompositingAllowed
#else
#define MAYBE_AcceleratedCompositingAllowed AcceleratedCompositingAllowed
#endif

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, MAYBE_AcceleratedCompositingAllowed) {
  GpuFeatureType type =
      GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  EXPECT_EQ(type, 0);

  const base::FilePath url(FILE_PATH_LITERAL("feature_compositing.html"));
  RunEventTest(url, kSwapBuffersEvent, true);
}

// Flash Stage3D may be blacklisted for other reasons on XP, so ignore it.
GpuFeatureType IgnoreGpuFeatures(GpuFeatureType type) {
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return static_cast<GpuFeatureType>(type &
        ~content::GPU_FEATURE_TYPE_FLASH_STAGE3D);
#endif
  return type;
}

class AcceleratedCompositingBlockedTest : public GpuFeatureTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    GpuFeatureTest::SetUpInProcessBrowserTestFixture();
    const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_compositing\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
    SetupBlacklist(json_blacklist);
  }
};

#if (defined(OS_WIN) && defined(USE_AURA))
// Compositing is always on for Windows Aura.
#define MAYBE_AcceleratedCompositingBlocked DISABLED_AcceleratedCompositingBlocked
#else
#define MAYBE_AcceleratedCompositingBlocked AcceleratedCompositingBlocked
#endif

IN_PROC_BROWSER_TEST_F(AcceleratedCompositingBlockedTest,
    MAYBE_AcceleratedCompositingBlocked) {
  GpuFeatureType type =
      GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  type = IgnoreGpuFeatures(type);

  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING);

  const base::FilePath url(FILE_PATH_LITERAL("feature_compositing.html"));
  RunEventTest(url, kSwapBuffersEvent, false);
}

class AcceleratedCompositingTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
  }
};

#if defined(OS_WIN) && defined(USE_AURA)
// Compositing is always on for Windows Aura.
#define MAYBE_AcceleratedCompositingDisabled DISABLED_AcceleratedCompositingDisabled
#else
#define MAYBE_AcceleratedCompositingDisabled AcceleratedCompositingDisabled
#endif

IN_PROC_BROWSER_TEST_F(AcceleratedCompositingTest,
                       MAYBE_AcceleratedCompositingDisabled) {
// Compositing is always on for Windows Aura.
  const base::FilePath url(FILE_PATH_LITERAL("feature_compositing.html"));
  RunEventTest(url, kSwapBuffersEvent, false);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, WebGLAllowed) {
  GpuFeatureType type =
      GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  EXPECT_EQ(type, 0);

  const base::FilePath url(FILE_PATH_LITERAL("feature_webgl.html"));
  RunEventTest(url, kWebGLCreationEvent, true);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, WebGLBlocked) {
  const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  SetupBlacklist(json_blacklist);
  GpuFeatureType type =
      GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  type = IgnoreGpuFeatures(type);
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);

  const base::FilePath url(FILE_PATH_LITERAL("feature_webgl.html"));
  RunEventTest(url, kWebGLCreationEvent, false);
}

class WebGLTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    GpuFeatureTest::SetUpCommandLine(command_line);
#if !defined(OS_ANDROID)
    // On Android, WebGL is disabled by default
    command_line->AppendSwitch(switches::kDisableExperimentalWebGL);
#endif
  }
};

IN_PROC_BROWSER_TEST_F(WebGLTest, WebGLDisabled) {
  const base::FilePath url(FILE_PATH_LITERAL("feature_webgl.html"));
  RunEventTest(url, kWebGLCreationEvent, false);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, MultisamplingAllowed) {
  GpuFeatureType type =
      GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  EXPECT_EQ(type, 0);

  // Multisampling is not supported if running on top of osmesa.
  std::string use_gl = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kUseGL);
  if (use_gl == gfx::kGLImplementationOSMesaName)
    return;

  // Linux Intel uses mesa driver, where multisampling is not supported.
  // Multisampling is also not supported on virtualized mac os.
  std::vector<std::string> configs;
  configs.push_back("LINUX INTEL");
  configs.push_back("MAC VMWARE");
  if (GPUTestBotConfig::CurrentConfigMatches(configs))
    return;

  const base::FilePath url(FILE_PATH_LITERAL("feature_multisampling.html"));
  RunTest(url, "\"TRUE\"", true);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, MultisamplingBlocked) {
  // Multisampling fails on virtualized mac os.
  if (GPUTestBotConfig::CurrentConfigMatches("MAC VMWARE"))
    return;

  const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"blacklist\": [\n"
      "        \"multisampling\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  SetupBlacklist(json_blacklist);
  GpuFeatureType type =
      GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  type = IgnoreGpuFeatures(type);
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_MULTISAMPLING);

  const base::FilePath url(FILE_PATH_LITERAL("feature_multisampling.html"));
  RunTest(url, "\"FALSE\"", true);
}

class WebGLMultisamplingTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableGLMultisampling);
  }
};

IN_PROC_BROWSER_TEST_F(WebGLMultisamplingTest, MultisamplingDisabled) {
  // Multisampling fails on virtualized mac os.
  if (GPUTestBotConfig::CurrentConfigMatches("MAC VMWARE"))
    return;

  const base::FilePath url(FILE_PATH_LITERAL("feature_multisampling.html"));
  RunTest(url, "\"FALSE\"", true);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, Canvas2DAllowed) {
  // Accelerated canvas 2D is not supported on XP.
  if (GPUTestBotConfig::CurrentConfigMatches("XP"))
    return;

  GpuFeatureType type =
      GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  EXPECT_EQ(type, 0);

  const base::FilePath url(FILE_PATH_LITERAL("feature_canvas2d.html"));
  RunEventTest(url, kAcceleratedCanvasCreationEvent, true);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, Canvas2DBlocked) {
  const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_2d_canvas\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  SetupBlacklist(json_blacklist);
  GpuFeatureType type =
      GpuDataManager::GetInstance()->GetBlacklistedFeatures();
  type = IgnoreGpuFeatures(type);
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);

  const base::FilePath url(FILE_PATH_LITERAL("feature_canvas2d.html"));
  RunEventTest(url, kAcceleratedCanvasCreationEvent, false);
}

class Canvas2DDisabledTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableAccelerated2dCanvas);
  }
};

IN_PROC_BROWSER_TEST_F(Canvas2DDisabledTest, Canvas2DDisabled) {
  const base::FilePath url(FILE_PATH_LITERAL("feature_canvas2d.html"));
  RunEventTest(url, kAcceleratedCanvasCreationEvent, false);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest,
                       CanOpenPopupAndRenderWithWebGLCanvas) {
  const base::FilePath url(FILE_PATH_LITERAL("webgl_popup.html"));
  RunTest(url, "\"SUCCESS\"", false);
}

// crbug.com/176466
IN_PROC_BROWSER_TEST_F(GpuFeatureTest,
                       DISABLED_CanOpenPopupAndRenderWith2DCanvas) {
  const base::FilePath url(FILE_PATH_LITERAL("canvas_popup.html"));
  RunTest(url, "\"SUCCESS\"", false);
}

class ThreadedCompositorTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableThreadedCompositing);
  }
};

// http://crbug.com/157985
IN_PROC_BROWSER_TEST_F(ThreadedCompositorTest, DISABLED_ThreadedCompositor) {
  const base::FilePath url(FILE_PATH_LITERAL("feature_compositing.html"));
  RunEventTest(url, kSwapBuffersEvent, true);
}


#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
// http://crbug.com/162343: flaky on Windows and Mac, failing on ChromiumOS.
#define MAYBE_RafNoDamage DISABLED_RafNoDamage
#else
#define MAYBE_RafNoDamage RafNoDamage
#endif
IN_PROC_BROWSER_TEST_F(GpuFeatureTest, MAYBE_RafNoDamage) {
  trace_categories_ = "-test_*";
  const base::FilePath url(FILE_PATH_LITERAL("feature_raf_no_damage.html"));
  RunEventTest(url);

  if (!analyzer_.get())
    return;

  // Search for matching name on begin event or async_begin event (any begin).
  Query query_raf =
      (Query::EventPhaseIs(TRACE_EVENT_PHASE_BEGIN) ||
       Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_BEGIN)) &&
      Query::EventNameIs("___RafWithNoDamage___");
  TraceEventVector events;
  size_t num_events = analyzer_->FindEvents(query_raf, &events);

  trace_analyzer::RateStats stats;
  trace_analyzer::RateStatsOptions stats_options;
  stats_options.trim_min = stats_options.trim_max = num_events / 10;
  EXPECT_TRUE(trace_analyzer::GetRateStats(events, &stats, &stats_options));

  LOG(INFO) << "Number of RAFs: " << num_events <<
      " Mean: " << stats.mean_us <<
      " Min: " << stats.min_us <<
      " Max: " << stats.max_us <<
      " StdDev: " << stats.standard_deviation_us;

  // Expect that the average time between RAFs is more than 15ms. That will
  // indicate that the renderer is not simply spinning on RAF.
  EXPECT_GT(stats.mean_us, 15000.0);

  // Print out the trace events upon error to debug failures.
  if (stats.mean_us <= 15000.0) {
    fprintf(stderr, "\n\nTRACE JSON:\n\n%s\n\n", trace_events_json_.c_str());
  }
}

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(GpuFeatureTest, IOSurfaceReuse) {
  if (!IOSurfaceSupport::Initialize())
    return;

  const base::FilePath url(
      FILE_PATH_LITERAL("feature_compositing_static.html"));
  base::FilePath test_path = gpu_test_dir_.Append(url);
  ASSERT_TRUE(file_util::PathExists(test_path))
      << "Missing test file: " << test_path.value();

  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_path));

  gfx::Rect bounds = browser()->window()->GetBounds();
  gfx::Rect new_bounds = bounds;

  const char* create_event = "IOSurfaceImageTransportSurface::CreateIOSurface";
  const char* resize_event = "IOSurfaceImageTransportSurface::OnResize";
  const char* draw_event = "CompositingIOSurfaceMac::DrawIOSurface";
  Query find_creates = Query::MatchBeginName(create_event);
  Query find_resizes = Query::MatchBeginName(resize_event) &&
                       Query::EventHasNumberArg("old_width") &&
                       Query::EventHasNumberArg("new_width");
  Query find_draws = Query::MatchBeginName(draw_event) &&
                     Query::EventHasNumberArg("scale");

  const int roundup = 64;
  // A few resize values assuming a roundup of 64 pixels. The test will resize
  // by these values one at a time and verify that CreateIOSurface only happens
  // when the rounded width changes.
  int offsets[] = { 1, roundup - 1, roundup, roundup + 1, 2*roundup};
  int num_offsets = static_cast<int>(arraysize(offsets));
  int w_start = bounds.width();

  for (int offset_i = 0; offset_i < num_offsets; ++offset_i) {
    new_bounds.set_width(w_start + offsets[offset_i]);
    ASSERT_TRUE(ResizeAndWait(new_bounds, "gpu", "gpu", resize_event));

    TraceEventVector resize_events;
    analyzer_->FindEvents(find_resizes, &resize_events);
    for (size_t resize_i = 0; resize_i < resize_events.size(); ++resize_i) {
      const trace_analyzer::TraceEvent* resize = resize_events[resize_i];
      // Was a create allowed:
      int old_width = resize->GetKnownArgAsInt("old_width");
      int new_width = resize->GetKnownArgAsInt("new_width");
      bool expect_create = (old_width/roundup != new_width/roundup ||
                            old_width == 0);
      int expected_creates = expect_create ? 1 : 0;

      // Find the create event inside this resize event (if any). This will
      // determine if the resize triggered a reallocation of the IOSurface.
      double begin_time = resize->timestamp;
      double end_time = begin_time + resize->GetAbsTimeToOtherEvent();
      Query find_this_create = find_creates &&
          Query::EventTime() >= Query::Double(begin_time) &&
          Query::EventTime() <= Query::Double(end_time);
      TraceEventVector create_events;
      int num_creates = static_cast<int>(analyzer_->FindEvents(find_this_create,
                                                               &create_events));
      EXPECT_EQ(expected_creates, num_creates);

      // For debugging failures, print out the width and height of each resize:
      LOG(INFO) <<
          base::StringPrintf(
              "%d (resize offset %d): IOSurface width %d -> %d; Creates %d "
              "Expected %d", offset_i, offsets[offset_i],
              old_width, new_width, num_creates, expected_creates);
    }
  }
}
#endif

}  // namespace anonymous
