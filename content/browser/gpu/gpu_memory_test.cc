// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "net/base/net_util.h"

namespace {

// Observer to report GPU memory usage when requested.
class GpuMemoryBytesAllocatedObserver
    : public content::GpuDataManagerObserver {
 public:
  GpuMemoryBytesAllocatedObserver()
      : bytes_allocated_(0) {
  }

  virtual ~GpuMemoryBytesAllocatedObserver() {
  }

  virtual void OnGpuInfoUpdate() OVERRIDE {}

  virtual void OnVideoMemoryUsageStatsUpdate(
      const content::GPUVideoMemoryUsageStats& video_memory_usage_stats)
          OVERRIDE {
    bytes_allocated_ = video_memory_usage_stats.bytes_allocated;
    message_loop_runner_->Quit();
  }

  size_t GetBytesAllocated() {
    message_loop_runner_ = new content::MessageLoopRunner;
    content::GpuDataManager::GetInstance()->AddObserver(this);
    content::GpuDataManager::GetInstance()->
        RequestVideoMemoryUsageStatsUpdate();
    message_loop_runner_->Run();
    content::GpuDataManager::GetInstance()->RemoveObserver(this);
    message_loop_runner_ = NULL;
    return bytes_allocated_;
  }

 private:
  size_t bytes_allocated_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

class GpuMemoryTest : public content::ContentBrowserTest {
 public:
  GpuMemoryTest()
      : allow_tests_to_run_(false),
        has_used_first_shell_(false) {
  }
  virtual ~GpuMemoryTest() {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(content::DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableLogging);
    command_line->AppendSwitch(switches::kForceCompositingMode);
    // TODO: Use switches::kForceGpuMemAvailableMb to fix the memory limit
    // (this is failing some bots (probably because of differing meanings of
    // GPU_EXPORT)
    // Only run this on GPU bots for now. These tests should work with
    // any GPU process, but may be slow.
    if (command_line->HasSwitch(switches::kUseGpuInTests)) {
      allow_tests_to_run_ = true;
    }
    // Don't enable these tests on Android just yet (they use lots of memory and
    // may not be stable).
#if defined(OS_ANDROID)
    allow_tests_to_run_ = false;
#endif
  }

  enum PageType {
    PAGE_CSS3D,
    PAGE_WEBGL,
  };

  // Load a page and consume a specified amount of GPU memory.
  void LoadPage(content::Shell* shell_to_load,
                PageType page_type,
                size_t mb_to_use) {
    FilePath url;
    switch (page_type) {
      case PAGE_CSS3D:
        url = gpu_test_dir_.AppendASCII("mem_css3d.html");
        break;
      case PAGE_WEBGL:
        url = gpu_test_dir_.AppendASCII("mem_webgl.html");
        break;
    }

    content::NavigateToURL(shell_to_load, net::FilePathToFileURL(url));
    std::ostringstream js_call;
    js_call << "useGpuMemory(";
    js_call << mb_to_use;
    js_call << ");";
    content::DOMMessageQueue message_queue;
    std::string message;
    ASSERT_TRUE(content::ExecuteScript(
        shell_to_load->web_contents(), js_call.str()));
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"DONE_USE_GPU_MEMORY\"", message);
  }

  // Create a new tab.
  content::Shell* CreateNewTab() {
    // The ContentBrowserTest will create one shell by default, use that one
    // first so that we don't confuse the memory manager into thinking there
    // are more windows than there are.
    content::Shell* new_shell =
        has_used_first_shell_ ? CreateBrowser() : shell();
    has_used_first_shell_ = true;
    visible_shells_.insert(new_shell);
    return new_shell;
  }

  void SetTabBackgrounded(content::Shell* shell_to_background) {
    ASSERT_TRUE(
        visible_shells_.find(shell_to_background) != visible_shells_.end());
    visible_shells_.erase(shell_to_background);
    shell_to_background->web_contents()->WasHidden();
  }

  size_t GetMemoryUsageMbytes() {
    // TODO: This should wait until all effects of memory management complete.
    // We will need to wait until all
    // 1. pending commits from the main thread to the impl thread in the
    //    compositor complete (for visible compositors).
    // 2. allocations that the renderer's impl thread will make due to the
    //    compositor and WebGL are completed.
    // 3. pending GpuMemoryManager::Manage() calls to manage are made.
    // 4. renderers' OnMemoryAllocationChanged callbacks in response to
    //    manager are made.
    // Each step in this sequence can cause trigger the next (as a 1-2-3-4-1
    // cycle), so we will need to pump this cycle until it stabilizes.
    GpuMemoryBytesAllocatedObserver observer;
    observer.GetBytesAllocated();
    return observer.GetBytesAllocated() / 1048576;
  }

  bool AllowTestsToRun() const {
    return allow_tests_to_run_;
  }

 private:
  bool allow_tests_to_run_;
  std::set<content::Shell*> visible_shells_;
  bool has_used_first_shell_;
  FilePath gpu_test_dir_;
};

}  // namespace
