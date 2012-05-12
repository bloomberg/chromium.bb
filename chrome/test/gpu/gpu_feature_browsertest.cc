// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/test/trace_event_analyzer.h"
#include "base/version.h"
#include "chrome/browser/gpu_blacklist.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_switches.h"
#include "content/test/gpu/gpu_test_config.h"
#include "content/test/gpu/test_switches.h"
#include "net/base/net_util.h"
#include "ui/gl/gl_switches.h"
#if defined(OS_MACOSX)
#include "ui/surface/io_surface_support_mac.h"
#endif

using content::GpuDataManager;
using content::GpuFeatureType;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEventVector;

namespace {

typedef uint32 GpuResultFlags;
#define EXPECT_NO_GPU_PROCESS         GpuResultFlags(1<<0)
// Expect a SwapBuffers to occur (see gles2_cmd_decoder.cc).
#define EXPECT_GPU_SWAP_BUFFERS       GpuResultFlags(1<<1)

class GpuFeatureTest : public InProcessBrowserTest {
 public:
  GpuFeatureTest() : trace_categories_("test_gpu"), gpu_enabled_(false) {}

  virtual void SetUpInProcessBrowserTestFixture() {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    // This enables DOM automation for tab contents.
    EnableDOMAutomation();

    InProcessBrowserTest::SetUpCommandLine(command_line);

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
    command_line->AppendSwitchASCII(switches::kWindowSize, "400,300");
  }

  void SetupBlacklist(const std::string& json_blacklist) {
    GpuBlacklist* blacklist = GpuBlacklist::GetInstance();
    ASSERT_TRUE(blacklist->LoadGpuBlacklist(
        json_blacklist, GpuBlacklist::kAllOs));
    blacklist->UpdateGpuDataManager();
  }

  // If expected_reply is NULL, we don't check the reply content.
  void RunTest(const FilePath& url,
               const char* expected_reply,
               bool new_tab) {
#if defined(OS_LINUX) && !defined(NDEBUG)
    // Bypass tests on GPU Linux Debug bots.
    if (gpu_enabled_)
      return;
#endif

    FilePath test_path;
    test_path = gpu_test_dir_.Append(url);
    ASSERT_TRUE(file_util::PathExists(test_path))
        << "Missing test file: " << test_path.value();

    ui_test_utils::DOMMessageQueue message_queue;
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

  void RunTest(const FilePath& url, GpuResultFlags expectations) {
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

    using trace_analyzer::Query;
    using trace_analyzer::TraceAnalyzer;

    ASSERT_TRUE(tracing::BeginTracing(trace_categories_));

    // Have to use a new tab for the blacklist to work.
    RunTest(url, NULL, true);

    ASSERT_TRUE(tracing::EndTracing(&trace_events_json_));

    analyzer_.reset(TraceAnalyzer::Create(trace_events_json_));
    analyzer_->AssociateBeginEndEvents();
    TraceEventVector events;

    // This measurement is flaky, because the GPU process is sometimes
    // started before the test (always with force-compositing-mode on CrOS).
    if (expectations & EXPECT_NO_GPU_PROCESS) {
      EXPECT_EQ(0u, analyzer_->FindEvents(
          Query::MatchBeginName("OnGraphicsInfoCollected"), &events));
    }

    // Check for swap buffers if expected:
    if (expectations & EXPECT_GPU_SWAP_BUFFERS) {
      EXPECT_GT(analyzer_->FindEvents(Query::EventName() ==
                                      Query::String("SwapBuffers"), &events),
                size_t(0));
    }
  }

 protected:
  FilePath gpu_test_dir_;
  scoped_ptr<TraceAnalyzer> analyzer_;
  std::string trace_categories_;
  std::string trace_events_json_;
  bool gpu_enabled_;
};

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, AcceleratedCompositingAllowed) {
  GpuFeatureType type = GpuDataManager::GetInstance()->GetGpuFeatureType();
  EXPECT_EQ(type, 0);

  const FilePath url(FILE_PATH_LITERAL("feature_compositing.html"));
  RunTest(url, EXPECT_GPU_SWAP_BUFFERS);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, AcceleratedCompositingBlocked) {
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
  GpuFeatureType type = GpuDataManager::GetInstance()->GetGpuFeatureType();
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING);

  const FilePath url(FILE_PATH_LITERAL("feature_compositing.html"));
  RunTest(url, EXPECT_NO_GPU_PROCESS);
}

class AcceleratedCompositingTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
  }
};

IN_PROC_BROWSER_TEST_F(AcceleratedCompositingTest,
                       AcceleratedCompositingDisabled) {
  const FilePath url(FILE_PATH_LITERAL("feature_compositing.html"));
  RunTest(url, EXPECT_NO_GPU_PROCESS);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, WebGLAllowed) {
  GpuFeatureType type = GpuDataManager::GetInstance()->GetGpuFeatureType();
  EXPECT_EQ(type, 0);

  const FilePath url(FILE_PATH_LITERAL("feature_webgl.html"));
  RunTest(url, EXPECT_GPU_SWAP_BUFFERS);
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
  GpuFeatureType type = GpuDataManager::GetInstance()->GetGpuFeatureType();
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);

  const FilePath url(FILE_PATH_LITERAL("feature_webgl.html"));
  RunTest(url, EXPECT_NO_GPU_PROCESS);
}

class WebGLTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableExperimentalWebGL);
  }
};

IN_PROC_BROWSER_TEST_F(WebGLTest, WebGLDisabled) {
  const FilePath url(FILE_PATH_LITERAL("feature_webgl.html"));
  RunTest(url, EXPECT_NO_GPU_PROCESS);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, MultisamplingAllowed) {
  GpuFeatureType type = GpuDataManager::GetInstance()->GetGpuFeatureType();
  EXPECT_EQ(type, 0);

  // Multisampling is not supported if running on top of osmesa.
  std::string use_gl = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kUseGL);
  if (use_gl == gfx::kGLImplementationOSMesaName)
    return;

#if defined(OS_LINUX) || defined(OS_MACOSX)
  // Linux Intel uses mesa driver, where multisampling is not supported.
  // Multisampling is also not supported on virtualized mac os.
  GPUTestBotConfig test_bot;
  test_bot.LoadCurrentConfig(NULL);

  const std::vector<uint32>& gpu_vendor = test_bot.gpu_vendor();
#if defined(OS_LINUX)
  if (gpu_vendor.size() == 1 && gpu_vendor[0] == 0x8086)
    return;
#endif  // defined(OS_LINUX)

#if defined(OS_MACOSX)
  if (gpu_vendor.size() == 1 && gpu_vendor[0] == 0x15AD)
    return;
#endif  // defined(OS_MACOSX)

#endif  // defined(OS_LINUX) || defined(OS_MACOSX)

  const FilePath url(FILE_PATH_LITERAL("feature_multisampling.html"));
  RunTest(url, "\"TRUE\"", true);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, MultisamplingBlocked) {
#if defined(OS_MACOSX)
  // Multisampling fails on virtualized mac os.
  GPUTestBotConfig test_bot;
  test_bot.LoadCurrentConfig(NULL);

  const std::vector<uint32>& gpu_vendor = test_bot.gpu_vendor();
  if (gpu_vendor.size() == 1 && gpu_vendor[0] == 0x15AD)
    return;
#endif

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
  GpuFeatureType type = GpuDataManager::GetInstance()->GetGpuFeatureType();
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_MULTISAMPLING);

  const FilePath url(FILE_PATH_LITERAL("feature_multisampling.html"));
  RunTest(url, "\"FALSE\"", true);
}

class WebGLMultisamplingTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableGLMultisampling);
  }
};

IN_PROC_BROWSER_TEST_F(WebGLMultisamplingTest, MultisamplingDisabled) {
#if defined(OS_MACOSX)
  // Multisampling fails on virtualized mac os.
  GPUTestBotConfig test_bot;
  test_bot.LoadCurrentConfig(NULL);

  const std::vector<uint32>& gpu_vendor = test_bot.gpu_vendor();
  if (gpu_vendor.size() == 1 && gpu_vendor[0] == 0x15AD)
    return;
#endif

  const FilePath url(FILE_PATH_LITERAL("feature_multisampling.html"));
  RunTest(url, "\"FALSE\"", true);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, Canvas2DAllowed) {
#if defined(OS_WIN)
  // Accelerated canvas 2D is not supported on XP.
  GPUTestBotConfig test_bot;
  test_bot.LoadCurrentConfig(NULL);
  if (test_bot.os() == GPUTestConfig::kOsWinXP)
    return;
#endif

  GpuFeatureType type = GpuDataManager::GetInstance()->GetGpuFeatureType();
  EXPECT_EQ(type, 0);

  const FilePath url(FILE_PATH_LITERAL("feature_canvas2d.html"));
  RunTest(url, EXPECT_GPU_SWAP_BUFFERS);
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
  GpuFeatureType type = GpuDataManager::GetInstance()->GetGpuFeatureType();
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);

  const FilePath url(FILE_PATH_LITERAL("feature_canvas2d.html"));
  RunTest(url, EXPECT_NO_GPU_PROCESS);
}

class Canvas2DDisabledTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableAccelerated2dCanvas);
  }
};

IN_PROC_BROWSER_TEST_F(Canvas2DDisabledTest, Canvas2DDisabled) {
  const FilePath url(FILE_PATH_LITERAL("feature_canvas2d.html"));
  RunTest(url, EXPECT_NO_GPU_PROCESS);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest,
                       CanOpenPopupAndRenderWithWebGLCanvas) {
  const FilePath url(FILE_PATH_LITERAL("webgl_popup.html"));
  RunTest(url, "\"SUCCESS\"", false);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, CanOpenPopupAndRenderWith2DCanvas) {
  const FilePath url(FILE_PATH_LITERAL("canvas_popup.html"));
  RunTest(url, "\"SUCCESS\"", false);
}

class ThreadedCompositorTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableThreadedCompositing);
  }
};

// disabled in http://crbug.com/123503
IN_PROC_BROWSER_TEST_F(ThreadedCompositorTest, ThreadedCompositor) {
  const FilePath url(FILE_PATH_LITERAL("feature_compositing.html"));
  RunTest(url, EXPECT_GPU_SWAP_BUFFERS);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, RafNoDamage) {
  trace_categories_ = "-test_*";
  const FilePath url(FILE_PATH_LITERAL("feature_raf_no_damage.html"));
  RunTest(url, GpuResultFlags(0));

  if (!analyzer_.get())
    return;

  TraceEventVector events;
  size_t num_events = analyzer_->FindEvents(
      Query::MatchBeginName("___RafWithNoDamage___"), &events);

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

}  // namespace anonymous
