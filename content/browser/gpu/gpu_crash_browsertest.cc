// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"

namespace content {
class GpuCrashTest : public ContentBrowserTest {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }
  FilePath gpu_test_dir_;
};

IN_PROC_BROWSER_TEST_F(GpuCrashTest, MANUAL_Kill) {
  DOMMessageQueue message_queue;

  content::GpuDataManagerImpl::GetInstance()->
      DisableDomainBlockingFor3DAPIsForTesting();

  // Load page and wait for it to load.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  NavigateToURL(
      shell(),
      GetFileUrlWithQuery(
          gpu_test_dir_.AppendASCII("webgl.html"), "query=kill"));
  observer.Wait();

  GpuProcessHostUIShim* host =
      GpuProcessHostUIShim::GetOneInstance();
  ASSERT_TRUE(host);
  host->SimulateCrash();

  std::string m;
  ASSERT_TRUE(message_queue.WaitForMessage(&m));
  EXPECT_EQ("\"SUCCESS\"", m);
}

IN_PROC_BROWSER_TEST_F(GpuCrashTest, MANUAL_WebkitLoseContext) {
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
