// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "net/base/net_util.h"
#include "ui/gfx/gl/gl_switches.h"

namespace {

typedef uint32 GpuResultFlags;
#define EXPECT_NO_GPU_PROCESS         GpuResultFlags(0)
// Expect GPU process to be created.
#define EXPECT_GPU_PROCESS            GpuResultFlags(1<<0)
// Expect num_contexts_ to be created (onscreen or offscreen).
#define EXPECT_GPU_CONTEXTS           GpuResultFlags(1<<1)
// Expect a SwapBuffers to occur (see gles2_cmd_decoder.cc).
#define EXPECT_GPU_SWAP_BUFFERS       GpuResultFlags(1<<2)

class GpuFeatureTest : public InProcessBrowserTest {
 public:
  GpuFeatureTest()
      : num_contexts_(0),
        num_offscreen_contexts_(0) {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    // This enables DOM automation for tab contents.
    EnableDOMAutomation();
#if !defined(OS_MACOSX)
    CHECK(!command_line->HasSwitch(switches::kUseGL)) <<
        "kUseGL must not be set by test framework code!";
    command_line->AppendSwitchASCII(switches::kUseGL, "osmesa");
#endif
  }

  void SetupBlacklist(const std::string& json_blacklist) {
    scoped_ptr<Version> os_version(Version::GetVersionFromString("1.0"));
    GpuBlacklist* blacklist = new GpuBlacklist("1.0");

    ASSERT_TRUE(blacklist->LoadGpuBlacklist(
        json_blacklist, GpuBlacklist::kAllOs));
    GpuDataManager::GetInstance()->SetBuiltInGpuBlacklist(blacklist);
  }

  void RunTest(const FilePath& url, GpuResultFlags expectations) {
    using namespace trace_analyzer;

    FilePath test_path;
    PathService::Get(chrome::DIR_TEST_DATA, &test_path);
    test_path = test_path.Append(FILE_PATH_LITERAL("gpu"));
    test_path = test_path.Append(url);

    ASSERT_TRUE(file_util::PathExists(test_path))
        << "Missing test file: " << test_path.value();

    ASSERT_TRUE(tracing::BeginTracing("test_gpu"));

    ui_test_utils::DOMMessageQueue message_queue;
    // Have to use a new tab for the blacklist to work.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), net::FilePathToFileURL(test_path), NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    // Wait for message indicating the test has finished running.
    ASSERT_TRUE(message_queue.WaitForMessage(NULL));

    std::string json_events;
    ASSERT_TRUE(tracing::EndTracing(&json_events));

    scoped_ptr<TraceAnalyzer> analyzer(TraceAnalyzer::Create(json_events));
    analyzer->AssociateBeginEndEvents();
    TraceAnalyzer::TraceEventVector events;

    size_t num_gpu_processes = (expectations & EXPECT_GPU_PROCESS) ? 1 : 0;
    analyzer->FindEvents(Query::MatchBeginName("OnGraphicsInfoCollected"),
                         &events);
    EXPECT_EQ(num_gpu_processes, events.size());

    // Check for context creation if expected:
    if (expectations & EXPECT_GPU_CONTEXTS) {
      analyzer->FindEvents(
          Query(EVENT_NAME) == Query::String("TryCreateGLContext"),
          &events);
      EXPECT_EQ(num_contexts_, events.size());
      analyzer->FindEvents(
          Query(EVENT_NAME) == Query::String("CreateGLContextSuccess"),
          &events);
      EXPECT_EQ(num_contexts_, events.size());
    }

    // Check for swap buffers if expected:
    if (expectations & EXPECT_GPU_SWAP_BUFFERS) {
      analyzer->FindEvents(
          Query(EVENT_NAME) == Query::String("SwapBuffers"),
          &events);
      EXPECT_GT(events.size(), size_t(0));
    }
  }

 protected:
  size_t num_contexts_;
  size_t num_offscreen_contexts_;
};

#if defined(OS_CHROMEOS)
// http://crbug.com/106259
#define MAYBE_AcceleratedCompositingAllowed DISABLED_testSetBooleanPrefTriggers
#else
#define MAYBE_AcceleratedCompositingAllowed AcceleratedCompositingAllowed
#endif
IN_PROC_BROWSER_TEST_F(GpuFeatureTest, MAYBE_AcceleratedCompositingAllowed) {
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(flags.flags(), 0u);

  num_contexts_ = 1;
  const FilePath url(FILE_PATH_LITERAL("feature_compositing.html"));
  RunTest(url, EXPECT_GPU_PROCESS | EXPECT_GPU_SWAP_BUFFERS |
          EXPECT_GPU_CONTEXTS);
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

IN_PROC_BROWSER_TEST_F(GpuFeatureTest, FLAKY_WebGLAllowed) {
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(flags.flags(), 0u);

  num_contexts_ = 2;
  const FilePath url(FILE_PATH_LITERAL("feature_webgl.html"));
  RunTest(url, EXPECT_GPU_PROCESS | EXPECT_GPU_SWAP_BUFFERS |
          EXPECT_GPU_CONTEXTS);
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

#if defined(OS_LINUX)
// http://crbug.com/104142
#define Canvas2DAllowed FLAKY_Canvas2DAllowed
#endif
IN_PROC_BROWSER_TEST_F(GpuFeatureTest, Canvas2DAllowed) {
  GpuFeatureFlags flags = GpuDataManager::GetInstance()->GetGpuFeatureFlags();
  EXPECT_EQ(flags.flags(), 0u);

#if defined(OS_MACOSX)
  // TODO(zmo): enabling Mac when skia backend is enabled.
  const GpuResultFlags expectations = EXPECT_NO_GPU_PROCESS;
#else
  num_contexts_ = 2;
  const GpuResultFlags expectations = EXPECT_GPU_PROCESS |
                                      EXPECT_GPU_SWAP_BUFFERS |
                                      EXPECT_GPU_CONTEXTS;
#endif
  const FilePath url(FILE_PATH_LITERAL("feature_canvas2d.html"));
  RunTest(url, expectations);
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

}  // namespace anonymous

