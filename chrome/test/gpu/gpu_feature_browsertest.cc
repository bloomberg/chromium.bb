// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "base/version.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_test_config.h"
#include "net/base/filename_util.h"
#include "ui/gl/gl_implementation.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using content::GpuDataManager;
using gpu::GpuFeatureType;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEventVector;

namespace {

const char kAcceleratedCanvasCreationEvent[] = "Canvas2DLayerBridgeCreation";
const char kWebGLCreationEvent[] = "DrawingBufferCreation";

class FakeContentClient : public content::ContentClient {
};

class GpuFeatureTest : public InProcessBrowserTest {
 public:
  GpuFeatureTest() : category_patterns_("test_gpu") {}

  virtual void SetUp() OVERRIDE {
    content::SetContentClient(&content_client_);
  }

  virtual void TearDown() OVERRIDE {
    content::SetContentClient(NULL);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    base::FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kDisablePopupBlocking);
    command_line->AppendSwitchASCII(switches::kWindowSize, "400,300");
  }

  void SetupBlacklist(const std::string& json_blacklist) {
    gpu::GPUInfo gpu_info;
    GpuDataManager::GetInstance()->InitializeForTesting(
        json_blacklist, gpu_info);
  }

  // If expected_reply is NULL, we don't check the reply content.
  void RunTest(const base::FilePath& url,
               const char* expected_reply,
               bool new_tab) {
#if defined(OS_LINUX) && !defined(NDEBUG)
    // Bypass tests on GPU Linux Debug bots.
    if (gfx::GetGLImplementation() != gfx::kGLImplementationOSMesaGL)
      return;
#endif

    base::FilePath test_path;
    test_path = gpu_test_dir_.Append(url);
    ASSERT_TRUE(base::PathExists(test_path))
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
    if (gfx::GetGLImplementation() != gfx::kGLImplementationOSMesaGL)
      return;
#endif

    ASSERT_TRUE(tracing::BeginTracing(category_patterns_));

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
                     const char* category_patterns,
                     const char* wait_category,
                     const char* wait_event) {
    if (!tracing::BeginTracingWithWatch(category_patterns, wait_category,
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
  std::string category_patterns_;
  std::string trace_events_json_;
  FakeContentClient content_client_;
};

class GpuFeaturePixelTest : public GpuFeatureTest {
 protected:
  virtual void SetUp() OVERRIDE {
    EnablePixelOutput();
    GpuFeatureTest::SetUp();
  }
};

class GpuCompositingBlockedTest : public GpuFeatureTest {
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
        "      \"features\": [\n"
        "        \"gpu_compositing\"\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}";
    SetupBlacklist(json_blacklist);
  }
};

IN_PROC_BROWSER_TEST_F(GpuCompositingBlockedTest, GpuCompositingBlocked) {
  EXPECT_TRUE(GpuDataManager::GetInstance()->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING));
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, WebGLAllowed) {
  EXPECT_FALSE(GpuDataManager::GetInstance()->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_WEBGL));

  // The below times out: http://crbug.com/166060
#if 0
  const base::FilePath url(FILE_PATH_LITERAL("feature_webgl.html"));
  RunEventTest(url, kWebGLCreationEvent, true);
#endif
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, WebGLBlocked) {
  const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"features\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  SetupBlacklist(json_blacklist);
  EXPECT_TRUE(GpuDataManager::GetInstance()->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_WEBGL));

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

// This test is oblivious to the fact that multisample could be blacklisted on
// some configurations. Previously disabled on GOOGLE_CHROME_BUILD and
// on OS_MACOSX: http://crbug.com/314745
IN_PROC_BROWSER_TEST_F(GpuFeatureTest, DISABLED_MultisamplingAllowed) {
  if (gpu::GPUTestBotConfig::GpuBlacklistedOnBot())
    return;
  // Multisampling is not supported if running on top of osmesa.
  if (gfx::GetGLImplementation() == gfx::kGLImplementationOSMesaGL)
    return;
  // Linux Intel uses mesa driver, where multisampling is not supported.
  // Multisampling is also not supported on virtualized mac os.
  std::vector<std::string> configs;
  configs.push_back("LINUX INTEL");
  configs.push_back("MAC VMWARE");
  if (gpu::GPUTestBotConfig::CurrentConfigMatches(configs))
    return;

  const base::FilePath url(FILE_PATH_LITERAL("feature_multisampling.html"));
  RunTest(url, "\"TRUE\"", true);
}

class WebGLMultisamplingTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable_multisampling");
  }
};

IN_PROC_BROWSER_TEST_F(WebGLMultisamplingTest, MultisamplingDisabled) {
  // Multisampling fails on virtualized mac os.
  if (gpu::GPUTestBotConfig::CurrentConfigMatches("MAC VMWARE"))
    return;

  const base::FilePath url(FILE_PATH_LITERAL("feature_multisampling.html"));
  RunTest(url, "\"FALSE\"", true);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, Canvas2DAllowed) {
  // Accelerated canvas 2D is not supported on XP.
  if (gpu::GPUTestBotConfig::CurrentConfigMatches("XP"))
    return;

  enum Canvas2DState {
    ENABLED,
    BLACKLISTED,  // Disabled via the blacklist.
    DISABLED,     // Not disabled via the blacklist, but expected to be disabled
                  // by configuration.
  } expected_state = ENABLED;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Blacklist rule #24 disables accelerated_2d_canvas on Linux.
  expected_state = BLACKLISTED;
#elif defined(OS_WIN)
  // Blacklist rule #67 disables accelerated_2d_canvas on XP.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    expected_state = BLACKLISTED;
#endif

  if (gpu::GPUTestBotConfig::GpuBlacklistedOnBot())
    expected_state = BLACKLISTED;

#if defined(USE_AURA)
  // Canvas 2D is always disabled in software compositing mode, make sure it is
  // marked as such if it wasn't blacklisted already.
  if (expected_state == ENABLED &&
      !content::GpuDataManager::GetInstance()->CanUseGpuBrowserCompositor()) {
    expected_state = DISABLED;
  }
#endif

  EXPECT_EQ(expected_state == BLACKLISTED,
            GpuDataManager::GetInstance()->IsFeatureBlacklisted(
                gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS));

  const base::FilePath url(FILE_PATH_LITERAL("feature_canvas2d.html"));
  RunEventTest(url, kAcceleratedCanvasCreationEvent, expected_state == ENABLED);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, Canvas2DBlocked) {
  const std::string json_blacklist =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"1.0\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"features\": [\n"
      "        \"accelerated_2d_canvas\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  SetupBlacklist(json_blacklist);
  EXPECT_TRUE(GpuDataManager::GetInstance()->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS));

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

IN_PROC_BROWSER_TEST_F(GpuFeaturePixelTest,
                       CanOpenPopupAndRenderWithWebGLCanvas) {
  if (gpu::GPUTestBotConfig::GpuBlacklistedOnBot())
    return;

  const base::FilePath url(FILE_PATH_LITERAL("webgl_popup.html"));
  RunTest(url, "\"SUCCESS\"", false);
}

// crbug.com/176466
IN_PROC_BROWSER_TEST_F(GpuFeatureTest,
                       DISABLED_CanOpenPopupAndRenderWith2DCanvas) {
  const base::FilePath url(FILE_PATH_LITERAL("canvas_popup.html"));
  RunTest(url, "\"SUCCESS\"", false);
}

#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
// http://crbug.com/162343: flaky on Windows and Mac, failing on ChromiumOS.
#define MAYBE_RafNoDamage DISABLED_RafNoDamage
#else
#define MAYBE_RafNoDamage RafNoDamage
#endif
IN_PROC_BROWSER_TEST_F(GpuFeatureTest, MAYBE_RafNoDamage) {
  category_patterns_ = "-test_*";
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
  if (gpu::GPUTestBotConfig::GpuBlacklistedOnBot())
    return;

  const base::FilePath url(
      FILE_PATH_LITERAL("feature_compositing_static.html"));
  base::FilePath test_path = gpu_test_dir_.Append(url);
  ASSERT_TRUE(base::PathExists(test_path))
      << "Missing test file: " << test_path.value();

  ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_path));

  LOG(INFO) << "did navigate";
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
    LOG(INFO) << "before wait";
    ASSERT_TRUE(ResizeAndWait(new_bounds, "gpu", "gpu", resize_event));
    LOG(INFO) << "after wait";

    TraceEventVector resize_events;
    analyzer_->FindEvents(find_resizes, &resize_events);
    LOG(INFO) << "num rezize events = " << resize_events.size();
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
  LOG(INFO) << "finished test";
}
#endif

}  // namespace
