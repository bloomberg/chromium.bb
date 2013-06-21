// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <utility>
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/component_updater/test/component_patcher_mock.h"
#include "chrome/browser/component_updater/test/component_updater_service_unittest.h"
#include "chrome/browser/component_updater/test/test_installer.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_notification_tracker.h"
#include "content/test/net/url_request_prepackaged_interceptor.h"
#include "googleurl/src/gurl.h"
#include "libxml/globals.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::TestNotificationTracker;

// Verify that we can download and install a component and a differential
// update to that component. We do three loops; the final loop should do
// nothing.
// We also check that exactly 5 network requests are issued:
// 1- update check (response: v1 available)
// 2- download crx (v1)
// 3- update check (response: v2 available)
// 4- download differential crx (v1 to v2)
// 5- update check (response: no further update available)
TEST_F(ComponentUpdaterTest, DifferentialUpdate) {
  base::MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE);
  content::TestBrowserThread io_thread(BrowserThread::IO);

  io_thread.StartIOThread();
  file_thread.Start();

  content::URLLocalHostRequestPrepackagedInterceptor interceptor;

  VersionedTestInstaller installer;
  CrxComponent com;
  RegisterComponent(&com, kTestComponent_ihfo, Version("0.0"), &installer);

  const GURL expected_update_url_0(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D0.0%26fp%3D%26uc");
  const GURL expected_update_url_1(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D1.0%26fp%3D1%26uc");
  const GURL expected_update_url_2(
      "http://localhost/upd?extra=foo"
      "&x=id%3Dihfokbkgjpifnbbojhneepfflplebdkc%26v%3D2.0%26fp%3Df22%26uc");
  const GURL expected_crx_url_1(
      "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1.crx");
  const GURL expected_crx_url_1_diff_2(
      "http://localhost/download/ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx");

  interceptor.SetResponse(expected_update_url_0,
                          test_file("updatecheck_diff_reply_1.xml"));
  interceptor.SetResponse(expected_update_url_1,
                          test_file("updatecheck_diff_reply_2.xml"));
  interceptor.SetResponse(expected_update_url_2,
                          test_file("updatecheck_diff_reply_3.xml"));
  interceptor.SetResponse(expected_crx_url_1,
                          test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1.crx"));
  interceptor.SetResponse(
      expected_crx_url_1_diff_2,
      test_file("ihfokbkgjpifnbbojhneepfflplebdkc_1to2.crx"));

  test_configurator()->SetLoopCount(3);

  component_updater()->Start();
  message_loop.Run();

  EXPECT_EQ(0, static_cast<TestInstaller*>(com.installer)->error());
  EXPECT_EQ(2, static_cast<TestInstaller*>(com.installer)->install_count());

  EXPECT_EQ(5, interceptor.GetHitCount());

  component_updater()->Stop();
}
