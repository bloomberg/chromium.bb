// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"

namespace {

void SimulateGPUCrash(content::Shell* s) {
  LOG(ERROR) << "SimulateGPUCrash, before LoadURL";
  s->LoadURL(GURL(chrome::kChromeUIGpuCrashURL));
  LOG(ERROR) << "SimulateGPUCrash, after LoadURL";
}

} // namespace

namespace content {
class GPUCrashTest : public ContentBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    // GPU tests require gpu acceleration.
    // We do not care which GL backend is used.
    command_line->AppendSwitchASCII(switches::kUseGL, "any");
  }
  virtual void SetUpInProcessBrowserTestFixture() {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }
  FilePath gpu_test_dir_;
};

// Currently Kill times out on GPU bots: http://crbug.com/101513
IN_PROC_BROWSER_TEST_F(GPUCrashTest, MANUAL_Kill) {
  DOMMessageQueue message_queue;

  // Load page and wait for it to load.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  NavigateToURL(
      shell(),
      GetFileUrlWithQuery(
          gpu_test_dir_.AppendASCII("webgl.html"), "query=kill"));
  observer.Wait();

  scoped_ptr<Shell> shell(CreateBrowser());
  SimulateGPUCrash(shell.get());

  std::string m;
  ASSERT_TRUE(message_queue.WaitForMessage(&m));
  EXPECT_EQ("\"SUCCESS\"", m);
}


IN_PROC_BROWSER_TEST_F(GPUCrashTest, MANUAL_WebkitLoseContext) {
  DOMMessageQueue message_queue;

  NavigateToURL(
      shell(),
      GetFileUrlWithQuery(
          gpu_test_dir_.AppendASCII("webgl.html"),
          "query=WEBGL_lose_context"));

  std::string m;
  ASSERT_TRUE(message_queue.WaitForMessage(&m));
  EXPECT_EQ("\"SUCCESS\"", m);
}

} // namespace content
