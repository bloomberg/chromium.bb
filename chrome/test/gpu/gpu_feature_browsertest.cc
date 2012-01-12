// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/test/trace_event_analyzer.h"
#include "base/version.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_util.h"
#include "ui/gfx/gl/gl_switches.h"

namespace {

typedef uint32 GpuResultFlags;
#define EXPECT_NO_GPU_PROCESS         GpuResultFlags(1<<0)
// Expect a SwapBuffers to occur (see gles2_cmd_decoder.cc).
#define EXPECT_GPU_SWAP_BUFFERS       GpuResultFlags(1<<1)

class GpuFeatureTest : public InProcessBrowserTest {
 public:
  GpuFeatureTest() {}

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
    if (!command_line->HasSwitch("enable-gpu")) {
#if !defined(OS_MACOSX)
      CHECK(test_launcher_utils::OverrideGLImplementation(
          command_line, gfx::kGLImplementationOSMesaName)) <<
          "kUseGL must not be set by test framework code!";
#endif
    }
    command_line->AppendSwitch(switches::kDisablePopupBlocking);
  }

  void SetupBlacklist(const std::string& json_blacklist) {
    scoped_ptr<Version> os_version(Version::GetVersionFromString("1.0"));
    GpuBlacklist* blacklist = new GpuBlacklist("1.0");

    ASSERT_TRUE(blacklist->LoadGpuBlacklist(
        json_blacklist, GpuBlacklist::kAllOs));
    GpuDataManager::GetInstance()->SetGpuBlacklist(blacklist);
  }

  // If expected_reply is NULL, we don't check the reply content.
  void RunTest(const FilePath& url,
               const char* expected_reply,
               bool new_tab) {
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
    using trace_analyzer::Query;
    using trace_analyzer::TraceAnalyzer;

    ASSERT_TRUE(tracing::BeginTracing("test_gpu"));

    // Have to use a new tab for the blacklist to work.
    RunTest(url, NULL, true);

    std::string json_events;
    ASSERT_TRUE(tracing::EndTracing(&json_events));

    scoped_ptr<TraceAnalyzer> analyzer(TraceAnalyzer::Create(json_events));
    analyzer->AssociateBeginEndEvents();
    trace_analyzer::TraceEventVector events;

    // This measurement is flaky, because the GPU process is sometimes
    // started before the test (always with force-compositing-mode on CrOS).
    if (expectations & EXPECT_NO_GPU_PROCESS) {
      EXPECT_EQ(0u, analyzer->FindEvents(
          Query::MatchBeginName("OnGraphicsInfoCollected"), &events));
    }

    // Check for swap buffers if expected:
    if (expectations & EXPECT_GPU_SWAP_BUFFERS) {
      EXPECT_GT(analyzer->FindEvents(Query::EventName() ==
                                     Query::String("SwapBuffers"), &events),
                size_t(0));
    }
  }

 protected:
  FilePath gpu_test_dir_;
};

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, AcceleratedCompositingAllowed) {
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(flags.flags(), 0u);

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
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(
      flags.flags(),
      static_cast<uint32>(GpuFeatureFlags::kGpuFeatureAcceleratedCompositing));

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
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(flags.flags(), 0u);

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
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(
      flags.flags(),
      static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));

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
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(flags.flags(), 0u);

  // Multisampling is not supported if running on top of osmesa.
  std::string use_gl = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kUseGL);
  if (use_gl == gfx::kGLImplementationOSMesaName)
    return;

  const FilePath url(FILE_PATH_LITERAL("feature_multisampling.html"));
  RunTest(url, "\"TRUE\"", true);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, MultisamplingBlocked) {
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
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(
      flags.flags(),
      static_cast<uint32>(GpuFeatureFlags::kGpuFeatureMultisampling));

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
  const FilePath url(FILE_PATH_LITERAL("feature_multisampling.html"));
  RunTest(url, "\"FALSE\"", true);
}

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, Canvas2DAllowed) {
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(flags.flags(), 0u);

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
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(
      flags.flags(),
      static_cast<uint32>(GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas));

  const FilePath url(FILE_PATH_LITERAL("feature_canvas2d.html"));
  RunTest(url, EXPECT_NO_GPU_PROCESS);
}

class Canvas2DTest : public GpuFeatureTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    GpuFeatureTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableAccelerated2dCanvas);
  }
};

IN_PROC_BROWSER_TEST_F(Canvas2DTest, Canvas2DDisabled) {
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

}  // namespace anonymous

